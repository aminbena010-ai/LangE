/*
 * Runtime de async/await (§52).
 *
 * En POSIX cada funcion async corre en una fibra (ucontext)
 * sobre un planificador cooperativo FIFO de una sola hebra:
 *
 *   - llamar a una funcion async solo crea y encola la tarea;
 *   - `await` suspende la tarea actual y cede el control al
 *     planificador, que elige la siguiente tarea lista;
 *   - al terminar el programa se drenan las tareas que nadie
 *     espero (atexit).
 *
 * En Windows no hay ucontext: se conserva la ejecucion
 * ansiosa de la version 0.4 como respaldo.
 *
 * Las tareas se registran en el heap del runtime y se
 * liberan al terminar el programa, igual que listas, maps
 * y structs.
 */

#include "runtime.h"
#include "runtime_internal.h"

#include <stdlib.h>


#define RIVO_TASK_STACK_SIZE (512 * 1024)


static struct RivoTask *ready_head = NULL;
static struct RivoTask *ready_tail = NULL;
static struct RivoTask *current_task = NULL;
static int64_t live_tasks = 0;
static int drain_registered = 0;

#if !defined(_WIN32)
static ucontext_t scheduler_context;
static RivoTryState scheduler_try_state;
#endif


static void rivo_enqueue(struct RivoTask *task)
{
    if (task->queued)
        return;

    task->queued = 1;
    task->next_ready = NULL;

    if (ready_tail == NULL) {
        ready_head = task;
        ready_tail = task;
    } else {
        ready_tail->next_ready = task;
        ready_tail = task;
    }
}


static struct RivoTask *rivo_dequeue(void)
{
    struct RivoTask *task = ready_head;

    if (task == NULL)
        return NULL;

    ready_head = task->next_ready;

    if (ready_head == NULL)
        ready_tail = NULL;

    task->next_ready = NULL;
    task->queued = 0;

    return task;
}


void rivo_async_drain(void);


static void rivo_register_drain(void)
{
    if (drain_registered)
        return;

    drain_registered = 1;

    if (atexit(rivo_async_drain) != 0) {

        rivo_fatal(
            "RuntimeError",
            "no se pudo registrar el drenaje de tareas"
        );
    }
}


void *rivo_task_new(void)
{
    struct RivoTask *task =
        calloc(1, sizeof(struct RivoTask));

    if (task == NULL) {

        rivo_fatal(
            "MemoryError",
            "memoria insuficiente al crear una tarea"
        );

        return NULL;
    }

    task->state = 0;
    task->value = 0;

    rivo_heap_register(task, RIVO_HEAP_TASK);

    return task;
}


void rivo_task_prepare(void *handle, void *entry, int64_t arg_count)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "tarea nula al preparar"
        );

        return;
    }

    struct RivoTask *task = handle;

    task->entry = entry;
    task->arg_count = arg_count < 0 ? 0 : arg_count;

    task->args =
        calloc(
            task->arg_count > 0 ? (size_t)task->arg_count : 1,
            sizeof(int64_t)
        );

    if (task->args == NULL) {

        rivo_fatal(
            "MemoryError",
            "memoria insuficiente para los argumentos de la tarea"
        );
    }
}


int64_t *rivo_task_args(void *handle)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "tarea nula al leer argumentos"
        );

        return NULL;
    }

    struct RivoTask *task = handle;

    if (task->args == NULL) {

        task->args =
            calloc(
                task->arg_count > 0 ? (size_t)task->arg_count : 1,
                sizeof(int64_t)
            );

        if (task->args == NULL) {

            rivo_fatal(
                "MemoryError",
                "memoria insuficiente para los argumentos de la tarea"
            );

            return NULL;
        }
    }

    return task->args;
}


#if !defined(_WIN32)

static void rivo_task_trampoline(void)
{
    struct RivoTask *task = current_task;

    if (task == NULL || task->entry == NULL) {

        rivo_fatal(
            "RuntimeError",
            "fibra sin tarea asociada"
        );

        return;
    }

    task->entry(task);

    /*
     * El codigo generado siempre llama a rivo_task_finish.
     * Si el punto de entrada retorna (pruebas en C), se
     * completa con 0.
     */
    rivo_task_finish(task, 0);
}

#endif


void rivo_task_start(void *handle)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "tarea nula al iniciar"
        );

        return;
    }

    struct RivoTask *task = handle;

    if (task->started || task->state == 1)
        return;

    if (task->entry == NULL)
        return;

    task->started = 1;
    live_tasks++;

    rivo_register_drain();

#if defined(_WIN32)

    /*
     * Respaldo ansioso: el cuerpo corre de inmediato y
     * `rivo_task_finish` solo marca el resultado.
     */
    task->entry(task);

#else

    task->stack = malloc(RIVO_TASK_STACK_SIZE);

    if (task->stack == NULL) {

        rivo_fatal(
            "MemoryError",
            "memoria insuficiente para la pila de una tarea"
        );

        return;
    }

    if (getcontext(&task->context) != 0) {

        rivo_fatal(
            "RuntimeError",
            "no se pudo crear el contexto de una tarea"
        );

        return;
    }

    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = RIVO_TASK_STACK_SIZE;
    task->context.uc_link = NULL;

    makecontext(&task->context, rivo_task_trampoline, 0);

    rivo_enqueue(task);

#endif
}


void rivo_task_finish(void *handle, int64_t value)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "tarea nula al completar"
        );

        return;
    }

    struct RivoTask *task = handle;

    task->value = value;
    task->state = 1;
    task->queued = 0;

    if (live_tasks > 0)
        live_tasks--;

#if !defined(_WIN32)

    if (current_task == task)
        current_task = NULL;

    /*
     * Cede el control al planificador. El contexto de la
     * fibra terminada no vuelve a reanudarse.
     */
    if (swapcontext(&task->context, &scheduler_context) != 0) {

        rivo_fatal(
            "RuntimeError",
            "no se pudo ceder el control al planificador"
        );
    }

    rivo_fatal(
        "RuntimeError",
        "una tarea completada intento reanudarse"
    );

#endif
}


void rivo_task_complete(void *handle, int64_t value)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "tarea nula al completar"
        );

        return;
    }

    struct RivoTask *task = handle;

    task->value = value;
    task->state = 1;
    task->queued = 0;

    if (task->started && live_tasks > 0)
        live_tasks--;
}


#if !defined(_WIN32)

static void rivo_task_yield(void)
{
    struct RivoTask *task = current_task;

    if (task == NULL) {

        rivo_fatal(
            "RuntimeError",
            "no hay una tarea en ejecucion para ceder"
        );

        return;
    }

    rivo_enqueue(task);

    if (swapcontext(&task->context, &scheduler_context) != 0) {

        rivo_fatal(
            "RuntimeError",
            "no se pudo ceder el control al planificador"
        );
    }
}


static int64_t rivo_run_until(struct RivoTask *target)
{
    while (target->state != 1) {

        struct RivoTask *next = rivo_dequeue();

        if (next == NULL) {

            rivo_fatal(
                "RuntimeError",
                "await sobre una tarea pendiente sin tareas listas (posible deadlock)"
            );

            return 0;
        }

        rivo_try_save(&scheduler_try_state);

        current_task = next;
        rivo_try_restore(&next->try_state);

        if (swapcontext(&scheduler_context, &next->context) != 0) {

            rivo_fatal(
                "RuntimeError",
                "no se pudo reanudar una tarea"
            );

            return 0;
        }

        rivo_try_save(&next->try_state);
        rivo_try_restore(&scheduler_try_state);

        current_task = NULL;
    }

    return target->value;
}

#endif


int64_t rivo_task_wait(void *handle)
{
    if (handle == NULL) {

        rivo_fatal(
            "RuntimeError",
            "await sobre una tarea nula"
        );

        return 0;
    }

    struct RivoTask *task = handle;

    if (task->state == 1)
        return task->value;

    if (!task->started) {

        rivo_fatal(
            "RuntimeError",
            "await sobre una tarea que nunca se inicio"
        );

        return 0;
    }

#if defined(_WIN32)

    /*
     * Con ejecucion ansiosa una tarea pendiente no puede
     * aparecer aqui salvo que nunca se iniciara.
     */
    rivo_fatal(
        "RuntimeError",
        "await sobre una tarea pendiente"
    );

    return 0;

#else

    if (current_task != NULL) {

        if (current_task == task) {

            rivo_fatal(
                "RuntimeError",
                "una tarea no puede esperarse a si misma"
            );

            return 0;
        }

        /*
         * Todo `await` es un punto de intercalado: se cede
         * una vez aunque la tarea ya haya terminado.
         */
        rivo_task_yield();

        while (task->state != 1)
            rivo_task_yield();

        return task->value;
    }

    return rivo_run_until(task);

#endif
}


int64_t rivo_task_state(void *handle)
{
    if (handle == NULL)
        return -1;

    struct RivoTask *task = handle;

    return task->state;
}


void rivo_async_drain(void)
{
#if !defined(_WIN32)

    /*
     * Solo drena cuando no hay una espera activa: en ese
     * caso el bucle de espera ya es el planificador.
     */
    if (current_task != NULL)
        return;

    struct RivoTask *next = rivo_dequeue();

    while (next != NULL) {

        rivo_try_save(&scheduler_try_state);

        current_task = next;
        rivo_try_restore(&next->try_state);

        if (swapcontext(&scheduler_context, &next->context) != 0) {

            rivo_fatal(
                "RuntimeError",
                "no se pudo drenar una tarea"
            );

            return;
        }

        rivo_try_save(&next->try_state);
        rivo_try_restore(&scheduler_try_state);

        current_task = NULL;
        next = rivo_dequeue();
    }

    if (live_tasks > 0) {

        rivo_fatal(
            "RuntimeError",
            "quedaron tareas pendientes sin posibilidad de avanzar"
        );
    }

#endif
}
