#ifndef RUNTIME_H
#define RUNTIME_H

/*
 * Runtime de E++.
 *
 * Este header NO depende del compilador (lexer, parser, AST,
 * semantic ni LLVM). Solo depende de la standard library de C.
 *
 * El runtime se compila aparte (librería estática) y se enlaza
 * contra los programas generados por `rivo`.
 *
 * Mapeo de tipos (claude.md §24):
 *
 *     int       -> i64    -> int64_t
 *     float   -> double -> double
 *     bool      -> i1     -> uint8_t (0 / 1, ampliado en codegen)
 *     char  -> i8     -> char
 *     string    -> i8*    -> const char *
 *
 * PROPIEDAD DE MEMORIA:
 *
 * - Los parámetros `const char *` de print/len son BORROWED:
 *   el runtime no los copia ni los libera.
 * - `rivo_input()` y `rivo_concat_string()` devuelven bloques
 *   de la ARENA del runtime: NO liberar. El runtime los
 *   libera al terminar el programa.
 */

#include <stdint.h>

/* =========================================================
 * print
 *
 * Escribe el valor y añade un salto de línea final.
 * bool se imprime como "true" / "false" (claude.md §6).
 * ======================================================= */

void rivo_print_int(int64_t value);

void rivo_print_float(double value);

void rivo_print_text(const char *value);

void rivo_print_bool(uint8_t value);

void rivo_print_char(char value);

void rivo_print_int_inline(int64_t value);
void rivo_print_float_inline(double value);
void rivo_print_text_inline(const char *value);
void rivo_print_bool_inline(uint8_t value);
void rivo_print_char_inline(char value);
void rivo_print_space(void);
void rivo_print_newline(void);

/* Reserva memoria para un struct y la libera al terminar el programa. */
void *rivo_struct_alloc(int64_t size);

int64_t rivo_floor_div_int(int64_t numerator, int64_t denominator);

/*
 * Aritmetica entera con comprobacion de desbordamiento
 * (arreglo 2026-10-16). Los operadores +, -, *, / y % de
 * enteros llaman a estas funciones en lugar de emitir
 * add/mul/sdiv/srem crudos de LLVM: un resultado fuera
 * del rango de int64_t aborta con OverflowError (o
 * ZeroDivisionError si el divisor es 0).
 */
int64_t rivo_add_int(int64_t left, int64_t right);
int64_t rivo_sub_int(int64_t left, int64_t right);
int64_t rivo_mul_int(int64_t left, int64_t right);
int64_t rivo_div_int(int64_t numerator, int64_t denominator);
int64_t rivo_mod_int(int64_t numerator, int64_t denominator);


/*
 * Aritmetica float IEEE 754 (arreglo 2026-10-16: se elimino
 * la rejilla decimal de 9 decimales). Los operadores float
 * +, -, *, /, % y la negacion llaman a estas funciones.
 */
double rivo_float_add(double left, double right);
double rivo_float_sub(double left, double right);
double rivo_float_mul(double left, double right);
double rivo_float_div(double left, double right);
double rivo_float_rem(double left, double right);
double rivo_float_neg(double value);


/* =========================================================
 * input
 *
 * Lee una línea desde stdin.
 *
 * - Elimina únicamente el salto de línea final si existe.
 * - Una línea vacía devuelve una cadena vacía ("").
 * - EOF sin leer ningún carácter devuelve la cadena vacía
 *   ("") — la variable E++ recibe "" en EOF (decisión de
 *   diseño 2026-10-04).
 * - EOF inmediatamente después de datos devuelve la línea
 *   parcial (sin salto de línea que eliminar).
 *
 * PROPIEDAD: el bloque pertenece a la arena del runtime y
 * se libera al terminar el programa. NO debe liberarse con
 * rivo_free_text(). Devuelve NULL solo si no hay memoria
 * disponible.
 * ======================================================= */

char *rivo_input(void);
char *rivo_input_prompt(const char *prompt);


/* Libera un bloque reservado por el llamador. NULL es seguro.
 * NO usar sobre resultados de rivo_input()/rivo_concat_string()
 * (doble liberación al salir). */
void rivo_free_text(char *text);


/*
 * Copia un C-string a la arena del runtime (para `-> string
 * owned` en FFI). Devuelve un puntero que NO debe liberarse
 * con rivo_free_text(): lo libera la arena al salir.
 */
char *rivo_string_copy_arena(const char *source);


/* =========================================================
 * len
 *
 * Longitud de un string en bytes (strlen). Para texto ASCII el
 * resultado coincide con el número de caracteres.
 * ======================================================= */

int64_t rivo_len_text(const char *text);


/* =========================================================
 * string + string (concatenación)
 *
 * Concatena `left` y `right` en un buffer nuevo.
 *
 * PROPIEDAD: el resultado pertenece a la arena del runtime
 * y se libera automáticamente al terminar el programa
 * (atexit). NO debe liberarse con rivo_free_text().
 *
 * Defensivo: un operador NULL se trata como cadena vacía.
 * Devuelve NULL si no hay memoria disponible.
 * ======================================================= */

char *rivo_concat_string(const char *left, const char *right);


/* =========================================================
 * Conversión a texto (f-strings)
 *
 * Las usa el codegen de `f"hola {valor}"` para convertir
 * cada interpolación en texto antes de concatenarla.
 *
 * PROPIEDAD: el bloque pertenece a la arena del runtime y
 * se libera al terminar el programa (igual que
 * rivo_concat_string). NO liberarlo con rivo_free_text().
 *
 * `float` usa el mismo formato que print (sin ceros
 * sobrantes, minimo 1 decimal, §53) para que
 * f"{x}" y print(x) muestren el mismo valor.
 * ======================================================= */

char *rivo_str_from_int(int64_t value);

char *rivo_str_from_float(double value);

char *rivo_str_from_bool(uint8_t value);

char *rivo_str_from_char(char value);
char *rivo_str_from_string(const char *text);


/* =========================================================
 * Etiquetas de tipo
 *
 * Los valores circulan por la frontera C <-> LLVM como
 * int64_t con el patrón de bits del valor real:
 *
 *     int       -> bits del entero
 *     float   -> bits de double (bitcast)
 *     bool      -> 0 / 1
 *     char  -> byte ampliado
 *     string    -> puntero ampliado
 *
 * La etiqueta dice al runtime cómo comparar y copiar.
 * El codegen usa estos mismos valores (llvm.c incluye
 * este header) para que no haya dos listas de tags.
 * ======================================================= */

enum {
    RIVO_TAG_INT = 1,
    RIVO_TAG_FLOAT = 2,
    RIVO_TAG_STRING = 3,
    RIVO_TAG_BOOL = 4,
    RIVO_TAG_CHAR = 5
};


/* =========================================================
 * list<T>
 *
 * Representación opaca: el codegen solo transporta el
 * puntero (claude.md §11: add, remove, len).
 *
 * Cada operación que puede fallar recibe `line`: la línea
 * original del programa E++ para el mensaje de error.
 *
 * PROPIEDAD: la lista y su buffer pertenecen al runtime y
 * se liberan al terminar el programa. NO liberarlos desde
 * el código generado.
 * ======================================================= */

typedef struct RivoList RivoList;


/* Crea una lista vacía con el tipo de sus elementos. */
RivoList *rivo_list_new(int64_t element_tag);


/* Añade un elemento al final. */
void rivo_list_add(RivoList *list, int64_t value);


/* Elimina la primera ocurrencia igual a `value`. */
void rivo_list_remove(RivoList *list, int64_t value);


/* Número de elementos. NULL devuelve 0. */
int64_t rivo_list_len(RivoList *list);


/*
 * Lee el elemento `index`.
 *
 * Fuera de rango: mensaje por stderr con la línea original
 * y terminación con código 1 (decisión de diseño
 * 2026-10-06: índice fuera de rango = error de runtime).
 */
int64_t rivo_list_get(RivoList *list, int64_t index, int64_t line);


/* Escribe el elemento `index`. Mismas reglas que rivo_list_get. */
void rivo_list_set(
    RivoList *list,
    int64_t index,
    int64_t value,
    int64_t line
);


/*
 * Concatenación `list + list` (claude.md §13).
 *
 * Devuelve una lista nueva con los elementos de `left`
 * seguidos de los de `right`; las dos entradas quedan
 * intactas. El resultado pertenece al heap del runtime.
 */
RivoList *rivo_concat_list(RivoList *left, RivoList *right);

/*
 * Builtins de lista (range, divmod, sorted, reversed,
 * all, any; 2026-10-10).
 */
RivoList *rivo_range(int64_t start, int64_t end);
RivoList *rivo_divmod(int64_t a, int64_t b);
RivoList *rivo_sorted(RivoList *list);
RivoList *rivo_reversed(RivoList *list);
uint8_t rivo_all(RivoList *list);
uint8_t rivo_any(RivoList *list);


/* =========================================================
 * map<K, V>
 *
 * Representación opaca. Las claves se comparan según el
 * tag: int/bool/char por igualdad exacta, float por
 * valor numérico y string por strcmp.
 *
 * Las claves string son BORROWED: apuntan a literales o a
 * bloques de la arena del runtime, que viven hasta el
 * final del programa (igual que print).
 * ======================================================= */

typedef struct RivoMap RivoMap;


/* Crea un map vacío con los tipos de clave y valor. */
RivoMap *rivo_map_new(int64_t key_tag, int64_t value_tag);


/* Inserta o sobrescribe la clave. */
void rivo_map_set(RivoMap *map, int64_t key, int64_t value);


/*
 * Lee el valor asociado a `key`.
 *
 * Clave inexistente: mensaje por stderr con la línea
 * original y terminación con código 1 (decisión de diseño
 * 2026-10-06).
 */
int64_t rivo_map_get(RivoMap *map, int64_t key, int64_t line);


/* Número de claves. `len(mapa)` y el for sobre map. */
int64_t rivo_map_len(RivoMap *map);


/*
 * Elimina la clave y su valor.
 *
 * El resto de claves conservan su orden de inserción.
 * Clave inexistente: sin efecto, la misma regla que
 * lista.remove() (claude.md §11/§12).
 */
void rivo_map_remove(RivoMap *map, int64_t key);


/*
 * Clave situada en la posición `position` (0..len-1).
 *
 * Uso interno del codegen: recorre las claves del map sin
 * copiarlas, de forma que el orden coincide con el de
 * inserción y no se reserva memoria auxiliar.
 */
int64_t rivo_map_key_at(RivoMap *map, int64_t position, int64_t line);


/* =========================================================
 * Aleatoriedad y tiempo (fase 2.1)
 *
 * Decisión de diseño 2026-10-06: firmas aprobadas por el
 * usuario. Los programas que necesiten números
 * reproducibles llaman a rivo_seed() al empezar; si nadie
 * llama a rivo_seed(), la primera tirada se siembra con el
 * reloj para que un juego no sea siempre igual.
 * ======================================================= */

/*
 * Entero aleatorio en [minimum, maximum] (ambos incluidos).
 *
 * maximum < minimum: error de runtime y terminación (1).
 */
int64_t rivo_random(int64_t minimum, int64_t maximum);

/* Float aleatorio en [0.0, 1.0). */
double rivo_random_float(void);

/* Fija la semilla de todos los aleatorios siguientes. */
void rivo_seed(int64_t value);

/* Milisegundos monótonos desde la primera llamada. */
int64_t rivo_time_ms(void);

/* Milisegundos desde la época (hora actual del sistema). */
int64_t rivo_epoch_ms(void);


/* =========================================================
 * Texto (fase 2.1)
 *
 * Todas devuelven strings de la arena: no se liberan desde
 * el programa E++.
 *
 * Los índices y longitudes son números de bytes y la
 * clasificación de mayúsculas es ASCII.
 * ======================================================= */

/* `separator` vacío: separa carácter a carácter. */
RivoList *rivo_split(const char *text, const char *separator);

/* Une los elementos de la lista con `separator`. */
char *rivo_join(RivoList *list, const char *separator);

char *rivo_upper(const char *text);
char *rivo_lower(const char *text);

/* Espacios por delante y por detrás. */
char *rivo_trim(const char *text);

/*
 * `length` de negativo: error de runtime. `start` fuera de
 * rango: error de runtime. Si `start + length` pasa del
 * final, se corta en el final (como en la mayoría de
 * lenguajes).
 */
char *rivo_substring(
    const char *text,
    int64_t start,
    int64_t length
);

/* `old_text` vacío: devuelve copia de `text`. */
char *rivo_replace(
    const char *text,
    const char *old_text,
    const char *new_text
);

uint8_t rivo_contains(const char *text, const char *part);
uint8_t rivo_starts_with(const char *text, const char *prefix);
uint8_t rivo_ends_with(const char *text, const char *suffix);

/* Posición del primer `part` o -1 si no está. */
int64_t rivo_find(const char *text, const char *part);

/*
 * Valor numerico del primer byte de la cadena (1..255).
 * Cadena vacia: ValueError.
 */
int64_t rivo_ord(const char *text);

/*
 * Cadena de un byte correspondiente al valor (1..255).
 * Fuera de rango: ValueError.
 */
char *rivo_chr(int64_t value);

/*
 * Conversion y numericos (builtins, 2026-10-10).
 */
int64_t rivo_to_int(const char *text);
double rivo_to_float(const char *text);
char *rivo_bin(int64_t value);
char *rivo_oct(int64_t value);
char *rivo_hex(int64_t value);
int64_t rivo_round(double value);
int64_t rivo_pow(int64_t base, int64_t exp);
int64_t rivo_abs(int64_t value);


/* =========================================================
 * Ficheros (fase 2.1)
 * ======================================================= */

/*
 * Lee el fichero completo como texto.
 *
 * No existe o no se puede leer: error de runtime y
 * terminación (1).
 */
char *rivo_read_file(const char *path);

/* 1 si se escribió completo, 0 si no se pudo abrir. */
uint8_t rivo_write_file(const char *path, const char *content);

/* 1 si el fichero se puede abrir para lectura. */
uint8_t rivo_file_exists(const char *path);


/* =========================================================
 * Consola (fase 2.1)
 *
 * clear() y move() escriben secuencias ANSI: en una
 * terminal quedan invisibles y en un fichero aparecen como
 * códigos de control, así que los tests deben fijar la
 * salida esperada con escape incluido.
 * ======================================================= */

/* Borra la pantalla y lleva el cursor a la posición 1,1. */
void rivo_clear(void);

/* Lleva el cursor a `row`, `column` (1-based; mínimo 1). */
void rivo_move(int64_t row, int64_t column);

/* Duerme `milliseconds` (los negativos no hacen nada). */
void rivo_sleep_ms(int64_t milliseconds);

/*
 * Lee una tecla sin esperar a Intro.
 *
 * En una terminal desactiva eco y línea durante la lectura;
 * con stdin en una tubería (tests) lee un byte normal.
 * Fin de fichero: devuelve el carácter 0.
 */
char rivo_read_key(void);


/* =========================================================
 * TRY / CATCH (claude.md §15)
 *
 * El codegen de E++ genera la llamada a setjmp() en el
 * propio marco de la función (la C estándar lo exige: un
 * setjmp en un marco ya devuelto es indefinido) y usa
 * esta pila para que rivo_fatal() encuentre el manejador
 * activo. Sin manejador, rivo_fatal se comporta como
 * siempre (mensaje por stderr y exit(1)).
 *
 * Decisión de diseño 2026-10-08 (usuario): se recogen
 * todos los aborts de runtime; `error` es string.
 * ======================================================= */

/*
 * Tamaño del búfer que el codegen reserva en el marco de
 * la función para el jmp_buf. static_assert en core.c
 * garantiza que jmp_buf cabe dentro.
 */
#define RIVO_TRY_BUFFER_SIZE 256

/* Profundidad máxima de manejadores activos. */
#define RIVO_TRY_MAX_DEPTH 64

/*
 * Apila `buffer` (con setjmp ya escrito en él por el
 * codegen) como manejador activo. Si la pila está llena,
 * aborta: no se puede registrar el manejador.
 */
void rivo_try_push(void *buffer);

/* Desapila el manejador superior. */
void rivo_try_pop(void);

/*
 * Mensaje del último error capturado (string del runtime,
 * sin el prefijo "rivo: "). Cadena estática del módulo.
 */
const char *rivo_try_error(void);


/* =========================================================
 * ASYNC / AWAIT (§52, incremento 2)
 *
 * Una tarea es un valor opaco del runtime. En sistemas
 * POSIX el cuerpo corre en una fibra (ucontext) y solo
 * avanza cuando el planificador FIFO la elige; `await`
 * suspende la tarea actual y devuelve el control al
 * planificador. En Windows se mantiene la ejecucion
 * ansiosa como respaldo.
 *
 * Estados:
 *     0 = pendiente
 *     1 = completada
 * ======================================================= */

/* Crea una tarea pendiente. No devuelve NULL (aborta si
 * no hay memoria). */
void *rivo_task_new(void);

/* Asocia el punto de entrada y reserva el array de
 * argumentos empaquetados (arg_count elementos int64). */
void rivo_task_prepare(void *task, void *entry, int64_t arg_count);

/* Array de argumentos empaquetados de la tarea. */
int64_t *rivo_task_args(void *task);

/* Encola la tarea para que el planificador la ejecute. */
void rivo_task_start(void *task);

/* Marca la tarea como completada desde su fibra y cede el
 * control al planificador. No regresa. */
void rivo_task_finish(void *task, int64_t value);

/* Guarda el resultado empaquetado (patron de bits int64)
 * y marca la tarea como completada. */
void rivo_task_complete(void *task, int64_t value);

/* Devuelve el resultado empaquetado. Si la tarea sigue
 * pendiente, ejecuta el planificador hasta que termine. */
int64_t rivo_task_wait(void *task);

/* Estado de la tarea: 0 pendiente, 1 completada. */
int64_t rivo_task_state(void *task);

/* Ejecuta las tareas encoladas que nadie espera. Se
 * registra con atexit al crear la primera tarea. */
void rivo_async_drain(void);

#endif /* RUNTIME_H */
