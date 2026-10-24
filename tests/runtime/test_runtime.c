/*
 * Test directo del runtime de E++ (sin pasar por el compilador).
 *
 * Comprueba print(), input(), len(), concat(), EOF, líneas
 * vacías, ownership (arena vs llamador), listas (add,
 * remove, len, get, set), maps (set, get, len, key_at) y
 * ausencia de leaks. Además cubre las funciones
 * incorporadas de la fase 2.1: aleatoriedad, tiempo,
 * texto y ficheros (consola y read_key se prueban desde
 * tests/run_tests.sh).
 *
 * La salida estándar se compara con expected_stdout.txt.
 * Los fallos de aserción van a stderr y provocan exit != 0.
 */

#include "runtime.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Declarada en runtime_internal.h, que los tests no usan. */
void rivo_fatal(const char *error_class, const char *format, ...);

static int failures = 0;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static void check_text(const char *got, const char *expected,
                       const char *message)
{
    if (got == NULL || expected == NULL ||
        strcmp(got, expected) != 0) {

        fprintf(
            stderr,
            "FAIL: %s (got=\"%s\", expected=\"%s\")\n",
            message,
            got != NULL ? got : "(null)",
            expected != NULL ? expected : "(null)"
        );
        failures++;
    }
}

static void test_len(void)
{
    check(rivo_len_text("hola") == 4, "len(\"hola\") == 4");
    check(rivo_len_text("") == 0, "len(\"\") == 0");
    check(rivo_len_text(NULL) == 0, "len(NULL) == 0 (defensivo)");
    check(rivo_len_text("E++") == 3, "len(\"E++\") == 3");
}

static void test_input(void)
{
    char *line1 = rivo_input();
    char *line2 = rivo_input();
    char *line3 = rivo_input();
    char *line4 = rivo_input();

    check_text(line1, "hola", "input() linea 1");
    check_text(line2, "", "input() linea vacia");
    check_text(line3, "fin", "input() linea 3 sin \\n final");
    check_text(line4, "", "input() en EOF devuelve cadena vacia");

    /*
     * line1..line4 pertenecen a la arena del runtime: NO se
     * liberan aquí. La arena se vacía al salir y ASan/LSan
     * del harness comprueba que no queda nada.
     */
    rivo_print_text(line1);
    rivo_print_text(line2);
    rivo_print_text(line3);
}

static void test_free_texto(void)
{
    /*
     * rivo_free_text() libera bloques del LLAMADOR (no los
     * de la arena). Aquí se reserva uno propio a mano.
     */
    char *own = malloc(sizeof("libre"));

    check(own != NULL, "malloc para test de free");
    memcpy(own, "libre", sizeof("libre"));

    check_text(own, "libre", "bloque propio");
    rivo_free_text(own);
    rivo_free_text(NULL); /* NULL: seguro */
}

static void test_print(void)
{
    rivo_print_text("Hola");
    rivo_print_int(42);
    rivo_print_int(-7);
    rivo_print_float(19.99);
    rivo_print_bool(1);
    rivo_print_bool(0);
    rivo_print_char('A');
}

static void test_concat(void)
{
    /*
     * El resultado pertenece a la arena del runtime:
     * NO se libera aquí. La arena se vacía al salir y
     * ASan/LSan del harness comprueba que no queda nada.
     */
    char *joined = rivo_concat_string("Hola ", "mundo");

    check_text(joined, "Hola mundo", "concat basica");
    check(rivo_len_text(joined) == 10, "len(concat) == 10");

    check_text(
        rivo_concat_string("", "solo"),
        "solo",
        "concat con izquierda vacia"
    );

    check_text(
        rivo_concat_string("solo", ""),
        "solo",
        "concat con derecha vacia"
    );

    check_text(
        rivo_concat_string(NULL, "x"),
        "x",
        "concat con NULL defensivo"
    );

    char *chained = rivo_concat_string(joined, "!");

    check_text(chained, "Hola mundo!", "concat encadenada");

    rivo_print_text(joined);
    rivo_print_text(chained);
}

static void test_listas(void)
{
    RivoList *list = rivo_list_new(RIVO_TAG_INT);

    check(list != NULL, "rivo_list_new devuelve memoria");
    check(rivo_list_len(list) == 0, "lista nueva vacia");

    rivo_list_add(list, 10);
    rivo_list_add(list, 20);
    rivo_list_add(list, 20);

    check(rivo_list_len(list) == 3, "len tras tres add");
    check(rivo_list_get(list, 0, 1) == 10, "get(0)");
    check(rivo_list_get(list, 2, 1) == 20, "get(2)");

    rivo_list_remove(list, 20);

    check(rivo_list_len(list) == 2, "remove acorta la lista");
    check(rivo_list_get(list, 1, 1) == 20,
          "remove quita solo la primera ocurrencia");

    rivo_list_set(list, 1, 30, 1);
    check(rivo_list_get(list, 1, 1) == 30, "set actualiza el elemento");

    rivo_list_remove(list, 999);

    check(rivo_list_len(list) == 2,
          "remove de un valor ausente no cambia nada");

    rivo_list_remove(list, 10);
    rivo_list_remove(list, 30);

    check(rivo_list_len(list) == 0, "lista vacia tras quitar todo");
    check(rivo_list_len(NULL) == 0, "len de lista nula es 0");
}


static void test_maps(void)
{
    RivoMap *map = rivo_map_new(RIVO_TAG_STRING, RIVO_TAG_INT);

    check(map != NULL, "rivo_map_new devuelve memoria");
    check(rivo_map_len(map) == 0, "map nuevo vacio");

    rivo_map_set(map, (int64_t) (intptr_t) "Ana", 20);
    rivo_map_set(map, (int64_t) (intptr_t) "Luis", 25);

    check(rivo_map_len(map) == 2, "len tras dos set");
    check(rivo_map_get(map, (int64_t) (intptr_t) "Ana", 1) == 20,
          "get Ana");
    check(rivo_map_get(map, (int64_t) (intptr_t) "Luis", 1) == 25,
          "get Luis");

    rivo_map_set(map, (int64_t) (intptr_t) "Ana", 21);

    check(rivo_map_len(map) == 2,
          "set sobre clave existente no añade entrada");
    check(rivo_map_get(map, (int64_t) (intptr_t) "Ana", 1) == 21,
          "set actualiza el valor");

    /* La igualdad de claves string es por contenido. */
    char other[] = "Luis";

    check(rivo_map_get(map, (int64_t) (intptr_t) other, 1) == 25,
          "clave string comparada por contenido");

    /* Orden de inserción: es el orden que recorre el for. */
    check_text(
        (const char *) (intptr_t) rivo_map_key_at(map, 0, 1),
        "Ana",
        "key_at(0) mantiene la primera clave"
    );

    check_text(
        (const char *) (intptr_t) rivo_map_key_at(map, 1, 1),
        "Luis",
        "key_at(1) es la segunda clave"
    );

    check(rivo_map_len(NULL) == 0, "len de map nulo es 0");
}


/* =========================================================
 * Funciones incorporadas (fase 2.1)
 * ======================================================= */

static void test_aleatorio(void)
{
    rivo_seed(42);
    int64_t first = rivo_random(1, 100);

    rivo_seed(42);
    int64_t second = rivo_random(1, 100);

    check(first == second,
          "rivo_seed(42) reproduce la misma tirada");

    int in_range = 1;

    for (int i = 0; i < 300; i++) {

        int64_t value = rivo_random(-5, 5);

        if (value < -5 || value > 5)
            in_range = 0;
    }

    check(in_range, "rivo_random se mantiene en [min,max]");

    check(rivo_random(7, 7) == 7,
          "rivo_random con min == max devuelve ese valor");

    double value = rivo_random_float();

    check(value >= 0.0 && value < 1.0,
          "rivo_random_float en [0.0, 1.0)");

    check(rivo_time_ms() >= 0, "rivo_time_ms no negativo");
    check(rivo_time_ms() >= 0, "rivo_time_ms sigue sin negativo");

    check(rivo_epoch_ms() > 1600000000000LL,
          "rivo_epoch_ms es la hora actual");
}


static void test_texto(void)
{
    check_text(rivo_upper("hola Mundo"),
               "HOLA MUNDO", "upper");
    check_text(rivo_lower("Hola Mundo"),
               "hola mundo", "lower");
    check_text(rivo_upper(""), "", "upper de cadena vacia");
    check_text(rivo_trim("   hola   "),
               "hola", "trim de espacios");
    check_text(rivo_trim(" \t\n"),
               "", "trim hasta vacio");

    check_text(rivo_substring("abcdef", 2, 3),
               "cde", "substring centrado");
    check_text(rivo_substring("abc", 0, 99),
               "abc", "substring se corta en el final");
    check_text(rivo_substring("abc", 3, 5),
               "", "substring al final da vacio");

    check_text(rivo_replace("banana", "an", "AN"),
               "bANANa", "replace de todas las ocurrencias");
    check_text(rivo_replace("abc", "", "x"),
               "abc", "replace con old vacio no cambia");
    check_text(rivo_replace("abc", "z", "y"),
               "abc", "replace sin ocurrencias no cambia");
    check_text(rivo_replace("aaa", "a", "bb"),
               "bbbbbb", "replace que alarga");

    check(rivo_contains("hola mundo", "mundo") == 1,
          "contains si esta");
    check(rivo_contains("hola", "x") == 0,
          "contains si no esta");
    check(rivo_starts_with("hola", "ho") == 1,
          "starts_with");
    check(rivo_starts_with("hola", "hola!") == 0,
          "starts_with con prefijo mas largo");
    check(rivo_ends_with("hola", "la") == 1, "ends_with");
    check(rivo_ends_with("hola", "la!") == 0,
          "ends_with con sufijo mas largo");
    check(rivo_find("hola", "la") == 2, "find devuelve indice");
    check(rivo_find("hola", "x") == -1, "find si no esta");
    check(rivo_find("hola", "") == 0,
          "find de cadena vacia es 0");

    RivoList *parts = rivo_split("a,b,c", ",");

    check(rivo_list_len(parts) == 3, "split en tres trozos");
    check_text((const char *) (intptr_t) rivo_list_get(parts, 1, 1),
               "b", "split[1]");

    RivoList *with_empty = rivo_split("a,,c", ",");

    check(rivo_list_len(with_empty) == 3,
          "split conserva los trozos vacios");
    check_text(
        (const char *) (intptr_t) rivo_list_get(with_empty, 1, 1),
        "", "el trozo del medio esta vacio"
    );

    RivoList *characters = rivo_split("ab", "");

    check(rivo_list_len(characters) == 2,
          "split con separador vacio da caracteres");

    check_text(rivo_join(parts, "-"),
               "a-b-c", "join de la lista");
    check_text(rivo_join(with_empty, ","),
               "a,,c", "join conserva trozos vacios");

    RivoList *empty = rivo_list_new(RIVO_TAG_STRING);

    check_text(rivo_join(empty, ","), "",
               "join de lista vacia da vacio");
}


static void test_ficheros(void)
{
    const char *path = "/tmp/rivo_test_runtime.txt";

    /* Por si una ejecución anterior lo dejó. */
    remove(path);

    check(rivo_file_exists(path) == 0,
          "el fichero de prueba no existe antes");

    check(rivo_write_file(path, "hola\nmundo") == 1,
          "write_file devuelve 1");

    check(rivo_file_exists(path) == 1,
          "file_exists tras escribir");

    check_text(rivo_read_file(path), "hola\nmundo",
               "roundtrip write_file/read_file");

    check(rivo_write_file(path, "") == 1,
          "write_file de cadena vacia");

    check_text(rivo_read_file(path), "",
               "read_file de fichero vacio");

    check(rivo_file_exists("/tmp/rivo_no_existe.txt") == 0,
          "file_exists de fichero inexistente");

    check(rivo_write_file(
              "/tmp/rivo_dir_inexistente/x.txt", "x") == 0,
          "write_file en directorio inexistente devuelve 0");

    remove(path);
}


static void test_consola(void)
{
    int64_t before = rivo_time_ms();

    rivo_sleep_ms(20);

    check(rivo_time_ms() - before >= 15,
          "sleep_ms espera el tiempo pedido");

    rivo_sleep_ms(-5); /* no hace nada */
    rivo_sleep_ms(0);  /* no hace nada */

    check(rivo_time_ms() >= before,
          "sleep_ms no retrocede el reloj");
}


static void test_try(void)
{
    /*
     * try/catch (2026-10-08): rivo_fatal con manejador
     * apilado debe hacer longjmp y dejar el mensaje en
     * rivo_try_error(); rivo_try_pop desapila.
     *
     * El búfer es estático para que el marco que recibe
     * el longjmp siga vivo y ASan no marque nada.
     */
    static jmp_buf buffer;

    rivo_try_push(&buffer);

    int jumped = setjmp(buffer);

    if (jumped == 0) {

        rivo_fatal("RuntimeError", "fallo %d", 42);

        check(0, "rivo_fatal debio hacer longjmp");
        rivo_try_pop();
        return;
    }

    check_text(rivo_try_error(), "fallo 42",
               "rivo_try_error devuelve el mensaje");

    rivo_try_pop();
}


static void test_tareas(void)
{
    void *task = rivo_task_new();

    check(task != NULL, "rivo_task_new devuelve una tarea");
    check(rivo_task_state(task) == 0,
          "la tarea nace pendiente");

    rivo_task_complete(task, 1234);

    check(rivo_task_state(task) == 1,
          "rivo_task_complete marca completada");
    check(rivo_task_wait(task) == 1234,
          "rivo_task_wait devuelve el valor");
}


#if !defined(_WIN32)

static char async_order[8];
static int async_order_len = 0;
static void *async_b_handle = NULL;


static void async_entry_b(void *handle)
{
    async_order[async_order_len++] = 'b';

    rivo_task_finish(handle, 2);
}


static void async_entry_a(void *handle)
{
    async_order[async_order_len++] = 'a';

    int64_t value = rivo_task_wait(async_b_handle);

    async_order[async_order_len++] = 'c';

    rivo_task_finish(handle, value + 1);
}


static void test_planificador(void)
{
    async_order_len = 0;

    void *a = rivo_task_new();
    void *b = rivo_task_new();

    async_b_handle = b;

    rivo_task_prepare(a, (void *)async_entry_a, 0);
    rivo_task_prepare(b, (void *)async_entry_b, 0);

    rivo_task_start(a);
    rivo_task_start(b);

    check(async_order_len == 0,
          "rivo_task_start no ejecuta el cuerpo");

    check(rivo_task_wait(a) == 3,
          "await dentro de una fibra devuelve el valor");

    check(async_order_len == 3 &&
          async_order[0] == 'a' &&
          async_order[1] == 'b' &&
          async_order[2] == 'c',
          "el planificador intercala las tareas");
}

#endif


int main(void)
{
    test_len();
    test_input();
    test_free_texto();
    test_print();
    test_concat();
    test_listas();
    test_maps();
    test_aleatorio();
    test_texto();
    test_ficheros();
    test_consola();
    test_try();
    test_tareas();

#if !defined(_WIN32)
    test_planificador();
#endif

    rivo_print_int(rivo_len_text("hola"));
    rivo_print_int(rivo_len_text(""));

    if (failures > 0) {
        fprintf(stderr, "RUNTIME TESTS: %d FALLOS\n", failures);
        return 1;
    }

    fprintf(stderr, "RUNTIME TESTS: OK\n");
    return 0;
}
