/*
 * Runtime de E++ — símbolos internos compartidos.
 *
 * En el antiguo src/runtime.c estas funciones eran static.
 * Al partir el runtime en varios ficheros de src/runtime
 * necesitan ser visibles desde los demás (listas, mapas,
 * texto y ficheros).
 */
#ifndef RIVO_RUNTIME_INTERNAL_H
#define RIVO_RUNTIME_INTERNAL_H

#include "runtime.h"

#include <stdint.h>

#if !defined(_WIN32)
#include <ucontext.h>
#endif

/*
 * Estructuras de colecciones: opacas en runtime.h para el
 * resto del compilador, pero visibles aquí para listas.c,
 * mapas.c y core.c.
 */
struct RivoList {
    int64_t element_tag;
    int64_t length;
    int64_t capacity;
    int64_t *items;
};


struct RivoMapEntry {
    int64_t key;
    int64_t value;
};


struct RivoMap {
    int64_t key_tag;
    int64_t value_tag;
    int64_t length;
    int64_t capacity;
    struct RivoMapEntry *entries;
};


enum {
    RIVO_HEAP_LIST = 1,
    RIVO_HEAP_MAP = 2,
    RIVO_HEAP_STRUCT = 3,
    RIVO_HEAP_TASK = 4
};


/*
 * Estado de try/catch de una fibra. El runtime usa una pila
 * global de manejadores; al cambiar de fibra hay que guardar
 * y restaurar la pila para que los longjmp no salten a otro
 * marco.
 */
typedef struct RivoTryState {
    void *handlers[RIVO_TRY_MAX_DEPTH];
    int depth;
    char message[1024];
} RivoTryState;


struct RivoTask {
    int state;
    int64_t value;

    /* Punto de entrada y argumentos empaquetados como int64. */
    void (*entry)(void *);
    int64_t *args;
    int64_t arg_count;
    int started;
    int queued;

#if !defined(_WIN32)
    ucontext_t context;
    void *stack;
    RivoTryState try_state;
#endif

    struct RivoTask *next_ready;
};

/* Arena de strings: definida en core.c. */
int rivo_arena_register(void *block);

/* Registro en el heap de listas, maps y structs: definida en core.c. */
void rivo_heap_register(void *block, int kind);

/* Guarda y restaura la pila de try/catch de la fibra actual. */
void rivo_try_save(RivoTryState *state);
void rivo_try_restore(const RivoTryState *state);

/*
 * Aborto con mensaje: definida en core.c.
 *
 * `error_class` es la clase estilo Python del fallo
 * (IndexError, KeyError, MemoryError, ...). Con un
 * manejador try activo, el catch recibe solo el mensaje
 * (sin la clase); fuera de try se imprime
 * "Clase: mensaje" por stderr y se sale con exit 1.
 */
void rivo_fatal(
    const char *error_class,
    const char *format,
    ...
);

/* Comparación por etiqueta de valores: core.c. */
int rivo_value_equal(int64_t tag, int64_t left, int64_t right);

#endif /* RIVO_RUNTIME_INTERNAL_H */
