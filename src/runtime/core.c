/*
 * Runtime de E++ — núcleo: print, input, len, concatenación,
 * errores y registro de valores del heap.
 *
 * Separado de src/runtime.c en la fase C (ver error.log).
 * Sin dependencias del compilador.
 */

#include "runtime.h"
#include "runtime_internal.h"

#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * float: IEEE 754 double (arreglo 2026-10-16)
 *
 * Se elimino la rejilla decimal de 9 decimales (§53): la
 * aritmética es la nativa del hardware, igual que en Python
 * y C. La muestra usa la representación corta que hace
 * round-trip exacto (estilo repr de Python).
 * ======================================================= */

static void rivo_format_float(double value, char *buffer, size_t size)
{
    if (value != value) {
        snprintf(buffer, size, "nan");
        return;
    }

    if (value == INFINITY) {
        snprintf(buffer, size, "inf");
        return;
    }

    if (value == -INFINITY) {
        snprintf(buffer, size, "-inf");
        return;
    }

    if (value == 0.0) {
        /* Normaliza -0.0 a "0.0". */
        snprintf(buffer, size, "0.0");
        return;
    }

    double magnitude = fabs(value);

    /*
     * Criterio de Python (repr): notación fija para
     * magnitudes en [1e-4, 1e16); científica fuera de ese
     * rango. Se elige la menor cantidad de dígitos que
     * reconstruye el mismo double (round-trip).
     */
    if (magnitude >= 1e-4 && magnitude < 1e16) {
        int precision;

        for (precision = 0; precision <= 17; precision++) {
            snprintf(buffer, size, "%.*f", precision, value);

            if (strtod(buffer, NULL) == value)
                break;
        }

        /*
         * Recorta ceros sobrantes tras el punto y garantiza
         * al menos un ".0" (100.0, no 100).
         */
        char *dot = strchr(buffer, '.');

        if (dot != NULL) {
            char *end = buffer + strlen(buffer) - 1;

            while (end > dot + 1 && *end == '0') {
                *end = '\0';
                end--;
            }

            if (end == dot) {
                end[1] = '0';
                end[2] = '\0';
            }
        } else {
            size_t length = strlen(buffer);

            if (length + 2 < size) {
                buffer[length] = '.';
                buffer[length + 1] = '0';
                buffer[length + 2] = '\0';
            }
        }

        return;
    }

    int precision;

    for (precision = 1; precision <= 16; precision++) {
        snprintf(buffer, size, "%.*e", precision, value);

        if (strtod(buffer, NULL) == value)
            break;
    }

    /*
     * Recorta ceros del mantisa: 2.0e-18 -> 2e-18 (como
     * el repr de Python). El exponente de %e ya tiene al
     * menos dos dígitos (e-05).
     */
    char *exponent_mark = strpbrk(buffer, "eE");

    if (exponent_mark != NULL && exponent_mark > buffer) {
        char mantissa[64];
        size_t mantissa_length =
            (size_t) (exponent_mark - buffer);

        if (mantissa_length >= sizeof(mantissa))
            mantissa_length = sizeof(mantissa) - 1;

        memcpy(mantissa, buffer, mantissa_length);
        mantissa[mantissa_length] = '\0';

        char *dot = strchr(mantissa, '.');

        if (dot != NULL) {
            char *end = mantissa + mantissa_length - 1;

            while (end > dot && *end == '0') {
                *end = '\0';
                end--;
            }

            if (end == dot)
                *dot = '\0';
        }

        char rest[64];
        snprintf(rest, sizeof(rest), "%s", exponent_mark);
        snprintf(buffer, size, "%s%s", mantissa, rest);
    }
}


/* =========================================================
 * print
 * ======================================================= */

void rivo_print_int(int64_t value)
{
    printf("%lld\n", (long long) value);
}


void rivo_print_float(double value)
{
    char buffer[512];

    rivo_format_float(value, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}


void rivo_print_text(const char *value)
{
    /*
     * Defensivo: un string NULL no debe tumbar el programa.
     * No es un comportamiento de lenguaje, solo robustez.
     */
    fputs(value != NULL ? value : "", stdout);
    fputc('\n', stdout);
}


void rivo_print_bool(uint8_t value)
{
    fputs(value != 0 ? "true" : "false", stdout);
    fputc('\n', stdout);
}


void rivo_print_char(char value)
{
    fputc((unsigned char) value, stdout);
    fputc('\n', stdout);
}

void rivo_print_int_inline(int64_t value)
{
    printf("%lld", (long long) value);
}

void rivo_print_float_inline(double value)
{
    char buffer[512];

    rivo_format_float(value, buffer, sizeof(buffer));
    fputs(buffer, stdout);
}

void rivo_print_text_inline(const char *value)
{
    fputs(value != NULL ? value : "", stdout);
}

void rivo_print_bool_inline(uint8_t value)
{
    fputs(value != 0 ? "true" : "false", stdout);
}

void rivo_print_char_inline(char value)
{
    fputc((unsigned char) value, stdout);
}

void rivo_print_space(void)
{
    fputc(' ', stdout);
}

void rivo_print_newline(void)
{
    fputc('\n', stdout);
}

int64_t rivo_floor_div_int(int64_t numerator, int64_t denominator)
{
    if (denominator == 0) {
        rivo_fatal("ZeroDivisionError", "division por cero");
        return 0;
    }

    if (numerator == INT64_MIN && denominator == -1) {
        rivo_fatal("OverflowError", "resultado fuera del rango de int");
        return 0;
    }

    int64_t quotient = numerator / denominator;
    int64_t remainder = numerator % denominator;

    if (remainder != 0 &&
        ((remainder < 0) != (denominator < 0))) {
        quotient--;
    }

    return quotient;
}


/*
 * Aritmetica entera con comprobacion de desbordamiento.
 *
 * Los operadores +, -, * y / de enteros se generan como
 * llamadas a estas funciones (arreglo 2026-10-16): antes
 * el codegen emitia add/mul/sdiv crudos de LLVM y un
 * desbordamiento envolvia en silencio (N = N * N llegaba
 * a 0 y el programa imprimia 0 para siempre).
 *
 * Si el resultado sale del rango de int64_t se aborta con
 * OverflowError (o ZeroDivisionError para divisores nulos),
 * coherente con rivo_floor_div_int() y con try/catch.
 *
 * Las comprobaciones son C portable (sin builtins de
 * GCC/Clang) para no romper el build con MSVC.
 */
static int rivo_add_overflows(int64_t left, int64_t right)
{
    if (right > 0 && left > INT64_MAX - right)
        return 1;

    if (right < 0 && left < INT64_MIN - right)
        return 1;

    return 0;
}


static int rivo_sub_overflows(int64_t left, int64_t right)
{
    if (right < 0 && left > INT64_MAX + right)
        return 1;

    if (right > 0 && left < INT64_MIN + right)
        return 1;

    return 0;
}


static int rivo_mul_overflows(int64_t left, int64_t right)
{
    if (left == 0 || right == 0)
        return 0;

    if (left == -1)
        return right == INT64_MIN;

    if (right == -1)
        return left == INT64_MIN;

    if (left > 0) {
        if (right > 0)
            return left > INT64_MAX / right;

        return right < INT64_MIN / left;
    }

    if (right > 0)
        return left < INT64_MIN / right;

    return left < INT64_MAX / right;
}


int64_t rivo_add_int(int64_t left, int64_t right)
{
    if (rivo_add_overflows(left, right))
        rivo_fatal("OverflowError",
            "suma fuera del rango de int");

    return left + right;
}


int64_t rivo_sub_int(int64_t left, int64_t right)
{
    if (rivo_sub_overflows(left, right))
        rivo_fatal("OverflowError",
            "resta fuera del rango de int");

    return left - right;
}


int64_t rivo_mul_int(int64_t left, int64_t right)
{
    if (rivo_mul_overflows(left, right))
        rivo_fatal("OverflowError",
            "multiplicacion fuera del rango de int");

    return left * right;
}


int64_t rivo_div_int(int64_t numerator, int64_t denominator)
{
    if (denominator == 0) {
        rivo_fatal("ZeroDivisionError", "division por cero");
        return 0;
    }

    if (numerator == INT64_MIN && denominator == -1) {
        rivo_fatal("OverflowError",
            "division fuera del rango de int");
        return 0;
    }

    return numerator / denominator;
}


int64_t rivo_mod_int(int64_t numerator, int64_t denominator)
{
    if (denominator == 0) {
        rivo_fatal("ZeroDivisionError", "division por cero");
        return 0;
    }

    /*
     * INT64_MIN % -1 es indefinido en C y el resto
     * matematico es 0; se devuelve directamente para no
     * invocar el operador % con ese par.
     */
    if (denominator == -1)
        return 0;

    return numerator % denominator;
}


/*
 * Aritmetica float IEEE 754 (sin rejilla decimal). Division
 * y resto por 0.0 siguen la semantica de C (inf/NaN).
 */
double rivo_float_add(double left, double right)
{
    return left + right;
}


double rivo_float_sub(double left, double right)
{
    return left - right;
}


double rivo_float_mul(double left, double right)
{
    return left * right;
}


double rivo_float_div(double left, double right)
{
    return left / right;
}


double rivo_float_rem(double left, double right)
{
    /* Truncamiento hacia cero (mismo criterio que el frem). */
    return fmod(left, right);
}


double rivo_float_neg(double value)
{
    return -value;
}


/* =========================================================
 * input
 * ======================================================= */

/*
 * Lee una línea desde stdin eliminando solo el '\n' final.
 *
 * EOF sin datos: devuelve la cadena vacía "" (decisión de
 * diseño 2026-10-04: la variable E++ recibe "" en EOF).
 * NULL únicamente en error de memoria.
 *
 * El bloque pertenece a la arena: NO liberar con
 * rivo_free_text().
 */
char *rivo_input_prompt(const char *prompt)
{
    size_t capacity = 128;
    size_t length = 0;

    if (prompt != NULL) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    char *buffer = malloc(capacity);

    if (buffer == NULL) {
        fputs(
            "MemoryError: memoria insuficiente en input()\n",
            stderr
        );
        return NULL;
    }

    for (;;) {

        int c = fgetc(stdin);

        if (c == EOF)
            break;

        if (c == '\n')
            break;

        /* Se necesita sitio para el carácter y luego el '\0'. */
        if (length + 2 > capacity) {

            size_t new_capacity = capacity * 2;
            char *grown = realloc(buffer, new_capacity);

            if (grown == NULL) {
                fputs(
                    "MemoryError: memoria insuficiente en input()\n",
                    stderr
                );
                free(buffer);
                return NULL;
            }

            buffer = grown;
            capacity = new_capacity;
        }

        buffer[length++] = (char) c;
    }

    buffer[length] = '\0';

    /*
     * EOF sin ningún carácter leído: length == 0, así que
     * aquí se devuelve "" en lugar de NULL.
     */
    rivo_arena_register(buffer);

    return buffer;
}


char *rivo_input(void)
{
    return rivo_input_prompt(NULL);
}


/*
 * Libera un bloque reservado directamente por el llamador
 * (ya no es el caso de rivo_input(): sus resultados van a
 * la arena). NULL es seguro.
 */
void rivo_free_text(char *text)
{
    free(text);
}


/* =========================================================
 * len
 * ======================================================= */

int64_t rivo_len_text(const char *text)
{
    if (text == NULL)
        return 0;

    return (int64_t) strlen(text);
}


/* =========================================================
 * string + string (concatenación)
 * ======================================================= */

/*
 * Arena de strings con heap.
 *
 * Todos los bloques registrados se liberan al terminar el
 * programa (atexit). Así la propiedad del resultado de la
 * concatenación no depende del programa E++: el runtime no
 * pierde memoria y el programador no necesita liberar nada.
 *
 * Decisión de diseño 2026-10-04 (ver error.log).
 */

typedef struct RivoArenaNode {
    void *block;
    struct RivoArenaNode *next;
} RivoArenaNode;

static RivoArenaNode *arena_head = NULL;
static int arena_registered = 0;


static void rivo_arena_free_all(void)
{
    RivoArenaNode *node = arena_head;
    arena_head = NULL;

    while (node != NULL) {
        RivoArenaNode *next = node->next;
        free(node->block);
        free(node);
        node = next;
    }
}


/*
 * Registra un bloque en la arena.
 *
 * Devuelve 0 si no hay memoria para el nodo de control. En
 * ese caso el bloque queda sin rastrear y no se liberará al
 * salir (lo recupera el sistema operativo al terminar el
 * proceso); se avisa por stderr.
 */
int rivo_arena_register(void *block)
{
    RivoArenaNode *node = malloc(sizeof(RivoArenaNode));

    if (node == NULL) {
        fputs(
            "SystemError: no se pudo registrar el string en la arena\n",
            stderr
        );
        return 0;
    }

    node->block = block;
    node->next = arena_head;
    arena_head = node;

    if (!arena_registered) {

        /*
         * atexit solo la primera vez. Si falla, la arena no
         * se liberará al salir: se avisa y se continúa.
         */
        if (atexit(rivo_arena_free_all) == 0) {
            arena_registered = 1;
        } else {
            fputs(
                "SystemError: no se pudo registrar la arena (atexit)\n",
                stderr
            );
        }
    }

    return 1;
}


char *rivo_concat_string(const char *left, const char *right)
{
    size_t left_length = (left != NULL) ? strlen(left) : 0;
    size_t right_length = (right != NULL) ? strlen(right) : 0;

    /*
     * left_length + right_length + 1 sin desbordar.
     * (right_length <= SIZE_MAX - 1 porque el '\0' también
     * ocupa espacio, así que el -1 no puede desbordar.)
     */
    if (left_length > SIZE_MAX - right_length - 1) {
        fputs(
            "MemoryError: memoria insuficiente en la concatenacion\n",
            stderr
        );
        return NULL;
    }

    size_t total = left_length + right_length + 1;
    char *result = malloc(total);

    if (result == NULL) {
        fputs(
            "MemoryError: memoria insuficiente en la concatenacion\n",
            stderr
        );
        return NULL;
    }

    if (left_length > 0)
        memcpy(result, left, left_length);

    if (right_length > 0)
        memcpy(result + left_length, right, right_length);

    result[total - 1] = '\0';

    rivo_arena_register(result);

    return result;
}


/* =========================================================
 * Conversión a texto (f-strings)
 *
 * Bloques de la arena del runtime: se liberan al terminar
 * el programa, igual que rivo_concat_string().
 * ======================================================= */

static char *rivo_str_arena_block(size_t length)
{
    char *block = malloc(length + 1);

    if (block == NULL) {
        fputs(
            "MemoryError: memoria insuficiente al convertir a texto\n",
            stderr
        );
        return NULL;
    }

    rivo_arena_register(block);

    return block;
}


char *rivo_str_from_int(int64_t value)
{
    char buffer[32];

    int written = snprintf(
        buffer,
        sizeof(buffer),
        "%lld",
        (long long) value
    );

    if (written < 0)
        return NULL;

    char *block = rivo_str_arena_block((size_t) written);

    if (block == NULL)
        return NULL;

    memcpy(block, buffer, (size_t) written + 1);

    return block;
}


char *rivo_str_from_float(double value)
{
    char buffer[512];

    rivo_format_float(value, buffer, sizeof(buffer));

    size_t length = strlen(buffer);

    char *block = rivo_str_arena_block(length);

    if (block == NULL)
        return NULL;

    memcpy(block, buffer, length + 1);

    return block;
}


char *rivo_str_from_bool(uint8_t value)
{
    const char *text = (value != 0) ? "true" : "false";

    size_t length = strlen(text);

    char *block = rivo_str_arena_block(length);

    if (block == NULL)
        return NULL;

    memcpy(block, text, length + 1);

    return block;
}


char *rivo_str_from_char(char value)
{
    char *block = rivo_str_arena_block(1);

    if (block == NULL)
        return NULL;

    block[0] = value;
    block[1] = '\0';

    return block;
}


/*
 * str(string) -> string: identidad. Los strings de E++
 * son inmutables (arena), devolver el mismo puntero es
 * correcto y evita una copia.
 */
char *rivo_str_from_string(const char *text)
{
    return (char *) text;
}


/* =========================================================
 * Builtins de conversion y numericos (2026-10-10)
 * ======================================================= */

int64_t rivo_to_int(const char *text)
{
    if (text == NULL)
        rivo_fatal("ValueError", "to_int(): texto nulo");

    char *end = NULL;
    long long value = strtoll(text, &end, 10);

    if (end == text || *end != '\0')
        rivo_fatal("ValueError",
            "to_int(): '%s' no es un entero valido", text);

    return (int64_t) value;
}


double rivo_to_float(const char *text)
{
    if (text == NULL)
        rivo_fatal("ValueError", "to_float(): texto nulo");

    char *end = NULL;
    double value = strtod(text, &end);

    if (end == text || *end != '\0')
        rivo_fatal("ValueError",
            "to_float(): '%s' no es un numero valido", text);

    return value;
}


static char *rivo_base_string(int64_t value, int base, const char *prefix)
{
    char buffer[80];
    int written = 0;
    uint64_t magnitude;

    if (value < 0) {
        buffer[written++] = '-';
        magnitude = (uint64_t) (-(value + 1)) + 1ULL;
    } else {
        magnitude = (uint64_t) value;
    }

    while (*prefix != '\0')
        buffer[written++] = *prefix++;

    if (magnitude == 0) {
        buffer[written++] = '0';
    } else {
        char digits[72];
        int n = 0;

        while (magnitude > 0) {
            int d = (int) (magnitude % (uint64_t) base);
            digits[n++] = (d < 10)
                ? (char) ('0' + d)
                : (char) ('a' + (d - 10));
            magnitude /= (uint64_t) base;
        }

        while (n > 0)
            buffer[written++] = digits[--n];
    }

    buffer[written] = '\0';

    char *block = rivo_str_arena_block((size_t) written);

    if (block == NULL)
        return NULL;

    memcpy(block, buffer, (size_t) written + 1);

    return block;
}


char *rivo_bin(int64_t value)
{
    return rivo_base_string(value, 2, "0b");
}


char *rivo_oct(int64_t value)
{
    return rivo_base_string(value, 8, "0o");
}


char *rivo_hex(int64_t value)
{
    return rivo_base_string(value, 16, "0x");
}


int64_t rivo_round(double value)
{
    if (value < 0.0)
        return (int64_t) (value - 0.5);

    return (int64_t) (value + 0.5);
}


int64_t rivo_pow(int64_t base, int64_t exp)
{
    if (exp < 0)
        rivo_fatal("ValueError",
            "pow(): el exponente %lld no puede ser negativo",
            (long long) exp);

    int64_t result = 1;
    int64_t b = base;
    int64_t e = exp;

    while (e > 0) {

        if (e & 1) {
            if (rivo_mul_overflows(result, b))
                rivo_fatal("OverflowError",
                    "pow(): resultado fuera del rango de int");
            result *= b;
        }

        e >>= 1;

        if (e > 0) {
            if (rivo_mul_overflows(b, b))
                rivo_fatal("OverflowError",
                    "pow(): resultado fuera del rango de int");
            b *= b;
        }
    }

    return result;
}


int64_t rivo_abs(int64_t value)
{
    if (value == INT64_MIN)
        rivo_fatal("ValueError", "abs(): desbordamiento con INT64_MIN");

    return value < 0 ? -value : value;
}


/* =========================================================
 * Errores de runtime
 *
 * Índice fuera de rango y clave inexistente de map son
 * errores de programa: mensaje por stderr con la línea
 * original y terminación con código 1.
 *
 * Decisión de diseño 2026-10-06 (ver error.log).
 *
 * TRY/CATCH (decisión de diseño 2026-10-08, usuario):
 * si hay un manejador activo (pila de rivo_try_push), el
 * abort se convierte en longjmp al catch; si no, el
 * comportamiento es el de siempre.
 * ======================================================= */

/*
 * El búfer del codegen (RIVO_TRY_BUFFER_SIZE) debe caber el
 * jmp_buf de la plataforma.
 */
_Static_assert(
    sizeof(jmp_buf) <= RIVO_TRY_BUFFER_SIZE,
    "RIVO_TRY_BUFFER_SIZE es menor que jmp_buf"
);


/*
 * Pila de manejadores: punteros a los búferes setjmp que
 * viven en los marcos de las funciones E++ generadas.
 * Solo se usa desde core.c (push/pop/fatal).
 */
static void *rivo_try_handlers[RIVO_TRY_MAX_DEPTH];

static int rivo_try_depth = 0;

static char rivo_try_message[1024];


void rivo_try_push(void *buffer)
{
    if (buffer == NULL)
        rivo_fatal("SystemError", "intento de registrar un manejador nulo");

    /*
     * Pila llena: no se puede registrar el manejador.
     * Se aborta SIN pasar por rivo_fatal (éste intentaría
     * longjmp a un manejador que no existe).
     */
    if (rivo_try_depth >= RIVO_TRY_MAX_DEPTH) {

        fputs(
            "RuntimeError: demasiados try anidados "
            "(limite del runtime)\n",
            stderr
        );

        exit(1);
    }

    rivo_try_handlers[rivo_try_depth] = buffer;
    rivo_try_depth++;
}


void rivo_try_pop(void)
{
    if (rivo_try_depth > 0)
        rivo_try_depth--;
}


const char *rivo_try_error(void)
{
    return rivo_try_message;
}


void rivo_try_save(RivoTryState *state)
{
    if (state == NULL)
        return;

    state->depth = rivo_try_depth;

    memcpy(
        state->handlers,
        rivo_try_handlers,
        sizeof(state->handlers)
    );

    memcpy(
        state->message,
        rivo_try_message,
        sizeof(state->message)
    );
}


void rivo_try_restore(const RivoTryState *state)
{
    if (state == NULL)
        return;

    rivo_try_depth = state->depth;

    memcpy(
        rivo_try_handlers,
        state->handlers,
        sizeof(state->handlers)
    );

    memcpy(
        rivo_try_message,
        state->message,
        sizeof(state->message)
    );
}


void rivo_fatal(
    const char *error_class,
    const char *format,
    ...
)
{
    va_list arguments;

    /*
     * Con manejador activo: el mensaje viaja por la pila
     * de buffers (sin la clase ni prefijo) y la ejecución
     * continúa en el catch vía longjmp. longjmp no
     * devuelve.
     */
    if (rivo_try_depth > 0) {

        va_start(arguments, format);

        vsnprintf(
            rivo_try_message,
            sizeof(rivo_try_message),
            format,
            arguments
        );

        va_end(arguments);

        longjmp(
            *(jmp_buf *) rivo_try_handlers[rivo_try_depth - 1],
            1
        );
    }

    fprintf(
        stderr,
        "%s: ",
        error_class != NULL
            ? error_class
            : "RuntimeError"
    );

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);

    fputc('\n', stderr);

    /*
     * exit() (y no abort()) para que se ejecuten los
     * manejadores de atexit: la arena de strings y los
     * valores del heap se liberan al salir.
     */
    exit(1);
}


/* =========================================================
 * Registro de valores del heap
 *
 * Toda lista, map y struct creados se liberan al terminar el
 * programa (atexit), igual que la arena de strings: el
 * programa E++ no gestiona memoria manualmente.
 *
 * struct RivoList, struct RivoMap y el enum RIVO_HEAP_* viven
 * en runtime_internal.h.
 * ======================================================= */

typedef struct RivoHeapNode {
    void *block;
    int kind;
    struct RivoHeapNode *next;
} RivoHeapNode;

static RivoHeapNode *heap_head = NULL;
static int heap_registered = 0;


static void rivo_heap_free_all(void)
{
    RivoHeapNode *node = heap_head;
    heap_head = NULL;

    while (node != NULL) {

        RivoHeapNode *next = node->next;

        if (node->kind == RIVO_HEAP_LIST) {

            struct RivoList *list = node->block;

            if (list != NULL)
                free(list->items);

        } else if (node->kind == RIVO_HEAP_MAP) {

            struct RivoMap *map = node->block;

            if (map != NULL)
                free(map->entries);

        } else if (node->kind == RIVO_HEAP_TASK) {

            struct RivoTask *task = node->block;

            if (task != NULL) {
                free(task->args);

#if !defined(_WIN32)
                free(task->stack);
#endif
            }
        }

        free(node->block);
        free(node);

        node = next;
    }
}


/*
 * Registra un valor del heap. Si el nodo de control no se puede
 * reservar se avisa por stderr y el valor queda sin
 * rastrear (el sistema operativo la recupera al salir).
 */
void rivo_heap_register(void *block, int kind)
{
    RivoHeapNode *node = malloc(sizeof(RivoHeapNode));

    if (node == NULL) {
        fputs(
            "SystemError: no se pudo registrar un valor del heap\n",
            stderr
        );
        return;
    }

    node->block = block;
    node->kind = kind;
    node->next = heap_head;
    heap_head = node;

    if (!heap_registered) {

        if (atexit(rivo_heap_free_all) == 0) {
            heap_registered = 1;
        } else {
            fputs(
                "SystemError: no se pudo registrar el registro "
                "de valores del heap (atexit)\n",
                stderr
            );
        }
    }
}


void *rivo_struct_alloc(int64_t size)
{
    if (size <= 0) {
        rivo_fatal(
            "ValueError",
            "el tamaño de un struct debe ser positivo"
        );
        return NULL;
    }

    void *block = calloc(1, (size_t) size);

    if (block == NULL) {
        rivo_fatal(
            "MemoryError",
            "memoria insuficiente al crear un struct"
        );
        return NULL;
    }

    rivo_heap_register(block, RIVO_HEAP_STRUCT);
    return block;
}


/* =========================================================
 * Comparación de valores por etiqueta
 *
 * Devuelve 1 si los dos patrones de bits representan el
 * mismo valor para el tipo `tag`.
 * ======================================================= */

int rivo_value_equal(int64_t tag, int64_t left, int64_t right)
{
    if (tag == RIVO_TAG_STRING) {

        const char *left_text = (const char *) (intptr_t) left;
        const char *right_text = (const char *) (intptr_t) right;

        if (left_text == NULL)
            left_text = "";

        if (right_text == NULL)
            right_text = "";

        return strcmp(left_text, right_text) == 0;
    }

    if (tag == RIVO_TAG_FLOAT) {

        double left_value;
        double right_value;

        memcpy(&left_value, &left, sizeof(double));
        memcpy(&right_value, &right, sizeof(double));

        return left_value == right_value;
    }

    return left == right;
}
