#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif


/* =========================================================
 * CONSTANTES INTERNAS
 * ========================================================= */

#define ERROR_INITIAL_CAPACITY 16
#define ERROR_DEFAULT_MAX_ERRORS 100


/* =========================================================
 * COLORES ANSI (sistema REM)
 *
 * Solo se emiten cuando stderr es un terminal real; en
 * pipes y ficheros (tests, LSP de ErrorE, redirecciones)
 * la salida queda en texto plano con el mismo formato.
 * NO_COLOR lo desactiva por completo.
 * ========================================================= */

#define REM_COLOR_RED     "\033[1;31m"
#define REM_COLOR_YELLOW  "\033[1;33m"
#define REM_COLOR_CYAN    "\033[1;36m"
#define REM_COLOR_BOLD    "\033[1m"
#define REM_COLOR_RESET   "\033[0m"

static int rem_color_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {

        const char *no_color = getenv("NO_COLOR");

        cached =
#ifdef _WIN32
            _isatty(_fileno(stderr)) &&
#else
            isatty(STDERR_FILENO) &&
#endif
            (no_color == NULL || no_color[0] == '\0')
                ? 1
                : 0;
    }

    return cached;
}


/* =========================================================
 * FUNCIONES INTERNAS
 * ========================================================= */

static char *error_strdup(const char *string)
{
    if (string == NULL)
        return NULL;

    size_t length = strlen(string);

    char *copy = malloc(length + 1);

    if (copy == NULL)
        return NULL;

    memcpy(copy, string, length + 1);

    return copy;
}


static int error_list_grow(ErrorList *list)
{
    if (list == NULL)
        return 0;

    int new_capacity;

    if (list->capacity == 0)
        new_capacity = ERROR_INITIAL_CAPACITY;
    else
        new_capacity = list->capacity * 2;

    CompilerError **new_errors = realloc(
        list->errors,
        sizeof(CompilerError *) * new_capacity
    );

    if (new_errors == NULL)
        return 0;

    list->errors = new_errors;
    list->capacity = new_capacity;

    return 1;
}


static void error_update_counters(
    ErrorContext *context,
    const CompilerError *error
)
{
    if (context == NULL || error == NULL)
        return;

    switch (error->severity) {

        case ERROR_WARNING:
            context->warning_count++;
            break;

        case ERROR_ERROR:
            context->error_count++;
            break;

        case ERROR_FATAL:
            context->error_count++;
            context->has_fatal_error = 1;
            break;

        case ERROR_INFO:
            break;
    }
}


static char *rem_line_copy(
    const char *source,
    int line
)
{
    if (source == NULL ||
        line <= 0)
        return NULL;

    const char *start = source;

    int current_line = 1;

    while (*start != '\0' && current_line < line) {

        if (*start == '\n')
            current_line++;

        start++;
    }

    if (current_line != line)
        return NULL;

    const char *end = start;

    while (*end != '\0' &&
           *end != '\n' &&
           *end != '\r') {

        end++;
    }

    size_t length = (size_t)(end - start);

    char *result = malloc(length + 1);

    if (result == NULL)
        return NULL;

    memcpy(result, start, length);
    result[length] = '\0';

    return result;
}


static char *error_get_line_copy(
    const ErrorContext *context,
    int line
)
{
    if (context == NULL)
        return NULL;

    return rem_line_copy(context->source, line);
}


/* =========================================================
 * CONTEXTO DE ERRORES
 * ========================================================= */

ErrorContext *error_context_create(
    const char *filename,
    const char *source
)
{
    ErrorContext *context = malloc(sizeof(ErrorContext));

    if (context == NULL)
        return NULL;

    context->filename = filename;
    context->source = source;

    context->errors.errors = NULL;
    context->errors.count = 0;
    context->errors.capacity = 0;

    context->max_errors = ERROR_DEFAULT_MAX_ERRORS;

    context->error_count = 0;
    context->warning_count = 0;
    context->has_fatal_error = 0;

    if (!error_list_grow(&context->errors)) {
        free(context);
        return NULL;
    }

    return context;
}


void error_context_destroy(ErrorContext *context)
{
    if (context == NULL)
        return;

    error_context_clear(context);

    free(context->errors.errors);

    context->errors.errors = NULL;
    context->errors.count = 0;
    context->errors.capacity = 0;

    free(context);
}


void error_context_clear(ErrorContext *context)
{
    if (context == NULL)
        return;

    for (int i = 0; i < context->errors.count; i++) {

        error_destroy(context->errors.errors[i]);

        context->errors.errors[i] = NULL;
    }

    context->errors.count = 0;

    context->error_count = 0;
    context->warning_count = 0;
    context->has_fatal_error = 0;
}


/* =========================================================
 * CREACIÓN DE ERRORES
 * ========================================================= */

CompilerError *error_create(
    ErrorKind kind,
    ErrorSeverity severity,
    ErrorPhase phase,
    ErrorLocation location,
    const char *message
)
{
    CompilerError *error = malloc(sizeof(CompilerError));

    if (error == NULL)
        return NULL;

    error->kind = kind;
    error->severity = severity;
    error->phase = phase;

    error->message = error_strdup(message);
    error->filename = NULL;

    error->location = location;

    error->source_line = NULL;

    error->length = 0;

    error->detail = NULL;
    error->suggestion = NULL;

    if (message != NULL && error->message == NULL) {
        free(error);
        return NULL;
    }

    return error;
}


void error_destroy(CompilerError *error)
{
    if (error == NULL)
        return;

    free(error->message);
    free(error->filename);
    free(error->source_line);
    free(error->detail);
    free(error->suggestion);

    error->message = NULL;
    error->filename = NULL;
    error->source_line = NULL;
    error->detail = NULL;
    error->suggestion = NULL;

    free(error);
}


/* =========================================================
 * REGISTRAR ERROR
 * ========================================================= */

void error_report(
    ErrorContext *context,
    CompilerError *error
)
{
    if (context == NULL || error == NULL)
        return;

    /*
     * Si ya hemos alcanzado el límite, no seguimos
     * acumulando errores normales.
     */
    if (error->severity != ERROR_FATAL &&
        error_limit_reached(context)) {

        error_destroy(error);
        return;
    }

    if (context->errors.count >= context->errors.capacity) {

        if (!error_list_grow(&context->errors)) {
            error_destroy(error);
            return;
        }
    }

    /*
     * Añadir nombre del archivo.
     */
    if (error->filename == NULL &&
        context->filename != NULL) {

        error->filename =
            error_strdup(context->filename);
    }

    /*
     * Añadir automáticamente la línea fuente.
     */
    if (error->source_line == NULL &&
        context->source != NULL &&
        error->location.line > 0) {

        error->source_line =
            error_get_line_copy(
                context,
                error->location.line
            );
    }

    context->errors.errors[
        context->errors.count
    ] = error;

    context->errors.count++;

    error_update_counters(context, error);
}


void error_report_simple(
    ErrorContext *context,
    ErrorKind kind,
    ErrorPhase phase,
    int line,
    int column,
    const char *message
)
{
    ErrorLocation location;

    location.line = line;
    location.column = column;
    location.offset = 0;

    CompilerError *error = error_create(
        kind,
        ERROR_ERROR,
        phase,
        location,
        message
    );

    if (error == NULL)
        return;

    error_report(context, error);
}


void error_report_full(
    ErrorContext *context,
    ErrorKind kind,
    ErrorPhase phase,
    int line,
    int column,
    int length,
    const char *message,
    const char *detail,
    const char *suggestion
)
{
    ErrorLocation location;

    location.line = line;
    location.column = column;
    location.offset = 0;

    CompilerError *error = error_create(
        kind,
        ERROR_ERROR,
        phase,
        location,
        message
    );

    if (error == NULL)
        return;

    error->length = length;

    if (detail != NULL) {
        error->detail = error_strdup(detail);
    }

    if (suggestion != NULL) {
        error->suggestion = error_strdup(suggestion);
    }

    error_report(context, error);
}


/* =========================================================
 * CONSULTAS
 * ========================================================= */

int error_count(const ErrorContext *context)
{
    if (context == NULL)
        return 0;

    return context->error_count;
}


int error_warning_count(const ErrorContext *context)
{
    if (context == NULL)
        return 0;

    return context->warning_count;
}


int error_has_errors(const ErrorContext *context)
{
    if (context == NULL)
        return 0;

    if (context->error_count > 0)
        return 1;

    if (context->warnings_as_errors &&
        context->warning_count > 0)
        return 1;

    return 0;
}


int error_has_fatal(const ErrorContext *context)
{
    if (context == NULL)
        return 0;

    return context->has_fatal_error;
}


const CompilerError *error_get(
    const ErrorContext *context,
    int index
)
{
    if (context == NULL)
        return NULL;

    if (index < 0 ||
        index >= context->errors.count)
        return NULL;

    return context->errors.errors[index];
}


/* =========================================================
 * LÍMITE DE ERRORES
 * ========================================================= */

void error_set_max_errors(
    ErrorContext *context,
    int max_errors
)
{
    if (context == NULL)
        return;

    if (max_errors < 0)
        max_errors = 0;

    context->max_errors = max_errors;
}


void error_set_warnings_as_errors(
    ErrorContext *context,
    int enabled
)
{
    if (context == NULL)
        return;

    context->warnings_as_errors = enabled ? 1 : 0;
}


int error_limit_reached(
    const ErrorContext *context
)
{
    if (context == NULL)
        return 0;

    /*
     * 0 significa sin límite.
     */
    if (context->max_errors == 0)
        return 0;

    return context->error_count >=
           context->max_errors;
}


/* =========================================================
 * CLASE DE ERROR
 *
 * Estilo Python: cada error se agrupa bajo una clase
 * conocida (SyntaxError, TypeError, NameError, ...) en
 * lugar de un codigo E####.
 * ========================================================= */

const char *error_kind_class(ErrorKind kind)
{
    switch (kind) {

        case ERROR_NONE:
            return "Error";

        /* -------------------------------------------------
         * LEXER y PARSER -> SyntaxError
         * ------------------------------------------------- */

        case ERROR_LEX_INVALID_CHARACTER:
        case ERROR_LEX_INVALID_NUMBER:
        case ERROR_LEX_INVALID_FLOAT:
        case ERROR_LEX_UNTERMINATED_STRING:
        case ERROR_LEX_UNTERMINATED_CHAR:
        case ERROR_LEX_INVALID_ESCAPE:
        case ERROR_LEX_INVALID_IDENTIFIER:
        case ERROR_LEX_INVALID_INDENTATION:
        case ERROR_LEX_UNEXPECTED_DEDENT:
        case ERROR_LEX_INVALID_OPERATOR:
        case ERROR_LEX_UNTERMINATED_COMMENT:
        case ERROR_LEX_UNTERMINATED_FSTRING:
        case ERROR_LEX_INVALID_FSTRING:

        case ERROR_PARSE_UNEXPECTED_TOKEN:
        case ERROR_PARSE_EXPECTED_TOKEN:
        case ERROR_PARSE_EXPECTED_IDENTIFIER:
        case ERROR_PARSE_EXPECTED_TYPE:
        case ERROR_PARSE_EXPECTED_EXPRESSION:
        case ERROR_PARSE_EXPECTED_STATEMENT:
        case ERROR_PARSE_EXPECTED_COLON:
        case ERROR_PARSE_EXPECTED_NEWLINE:
        case ERROR_PARSE_EXPECTED_INDENT:
        case ERROR_PARSE_EXPECTED_DEDENT:
        case ERROR_PARSE_INVALID_DECLARATION:
        case ERROR_PARSE_INVALID_FUNCTION:
        case ERROR_PARSE_INVALID_PARAMETER:
        case ERROR_PARSE_INVALID_IMPORT:
        case ERROR_PARSE_INVALID_EXPRESSION:
        case ERROR_PARSE_INVALID_ASSIGNMENT:
        case ERROR_PARSE_MISSING_RETURN_TYPE:
        case ERROR_PARSE_INVALID_TYPE:

        /* Saltos invalidos tambien son de sintaxis
         * (igual que en Python: 'break' outside loop). */

        case ERROR_SEM_INVALID_BREAK:
        case ERROR_SEM_INVALID_CONTINUE:
        case ERROR_SEM_RETURN_OUTSIDE_FUNCTION:
            return "SyntaxError";

        /* -------------------------------------------------
         * SEMANTICA: tipos
         * ------------------------------------------------- */

        case ERROR_SEM_TYPE_MISMATCH:
        case ERROR_SEM_INVALID_TYPE:
        case ERROR_SEM_INVALID_OPERATION:
        case ERROR_SEM_ARGUMENT_COUNT:
        case ERROR_SEM_ARGUMENT_TYPE:
        case ERROR_SEM_RETURN_TYPE:
        case ERROR_SEM_INVALID_INDEX:
        case ERROR_SEM_INVALID_ELEMENT_TYPE:
        case ERROR_SEM_INVALID_KEY_TYPE:
        case ERROR_SEM_NULL_NOT_ALLOWED:
        case ERROR_SEM_INVALID_ASSIGNMENT:
            return "TypeError";

        /* -------------------------------------------------
         * SEMANTICA: nombres
         * ------------------------------------------------- */

        case ERROR_SEM_UNDEFINED_NAME:
        case ERROR_SEM_DUPLICATE_NAME:
        case ERROR_SEM_VARIABLE_NOT_INITIALIZED:
        case ERROR_SEM_FUNCTION_NOT_FOUND:
            return "NameError";

        /* -------------------------------------------------
         * SEMANTICA: constantes
         * ------------------------------------------------- */

        case ERROR_SEM_ASSIGN_TO_CONST:
            return "ValueError";

        /* -------------------------------------------------
         * SEMANTICA: visibilidad
         * ------------------------------------------------- */

        case ERROR_SEM_PRIVATE_ACCESS:
            return "AttributeError";

        /* -------------------------------------------------
         * SEMANTICA: imports
         * ------------------------------------------------- */

        case ERROR_SEM_MODULE_NOT_FOUND:
        case ERROR_SEM_IMPORT_NOT_FOUND:
            return "ImportError";

        /* -------------------------------------------------
         * CODEGEN e INTERNAL -> SystemError
         * ------------------------------------------------- */

        case ERROR_CODEGEN_INVALID_AST:
        case ERROR_CODEGEN_INVALID_TYPE:
        case ERROR_CODEGEN_INVALID_OPERATION:
        case ERROR_CODEGEN_FUNCTION_ERROR:
        case ERROR_CODEGEN_VARIABLE_ERROR:
        case ERROR_CODEGEN_LLVM_ERROR:
        case ERROR_CODEGEN_TARGET_ERROR:

        case ERROR_INTERNAL_NULL_POINTER:
        case ERROR_INTERNAL_OUT_OF_MEMORY:
        case ERROR_INTERNAL_INVALID_STATE:
        case ERROR_INTERNAL_UNIMPLEMENTED:
            return "SystemError";

        default:
            return "Error";
    }
}


/* =========================================================
 * NOMBRE DE FASE
 * ========================================================= */

const char *error_phase_name(ErrorPhase phase)
{
    switch (phase) {

        case ERROR_PHASE_LEXER:
            return "lexer";

        case ERROR_PHASE_PARSER:
            return "parser";

        case ERROR_PHASE_SEMANTIC:
            return "semantic";

        case ERROR_PHASE_CODEGEN:
            return "codegen";

        case ERROR_PHASE_INTERNAL:
            return "internal";

        default:
            return "unknown";
    }
}


/* =========================================================
 * NOMBRE DE SEVERIDAD
 * ========================================================= */

const char *error_severity_name(
    ErrorSeverity severity
)
{
    switch (severity) {

        case ERROR_INFO:
            return "info";

        case ERROR_WARNING:
            return "warning";

        case ERROR_ERROR:
            return "error";

        case ERROR_FATAL:
            return "fatal";

        default:
            return "unknown";
    }
}


/* =========================================================
 * RENDER REM (interno)
 *
 * Pinta el bloque completo de un diagnostico:
 *
 *     TypeError: se esperaba int pero se encontro string
 *       --> programa.ro:2:9
 *        |
 *      2 | var edad:int = "hola"
 *        |                ^^^^^^
 *        |
 *        = detalle: ...
 *        = sugerencia: ...
 * ========================================================= */

static void rem_render(
    ErrorSeverity severity,
    const char *class_name,
    const char *filename,
    const char *source_line,
    int line,
    int column,
    int length,
    const char *message,
    const char *detail,
    const char *suggestion
)
{
    int color = rem_color_enabled();

    const char *accent = REM_COLOR_RED;

    if (severity == ERROR_WARNING)
        accent = REM_COLOR_YELLOW;

    /*
     * 1. Cabecera: Clase: mensaje
     */
    if (color) {

        fprintf(
            stderr,
            "%s%s: %s%s\n",
            accent,
            class_name,
            message != NULL
                ? message
                : "error desconocido",
            REM_COLOR_RESET
        );

    } else {

        fprintf(
            stderr,
            "%s: %s\n",
            class_name,
            message != NULL
                ? message
                : "error desconocido"
        );
    }

    /*
     * 2. Ubicación.
     */
    if (filename != NULL) {

        if (color) {

            fprintf(
                stderr,
                "  --> %s%s%s:%d:%d\n",
                REM_COLOR_BOLD,
                filename,
                REM_COLOR_RESET,
                line,
                column
            );

        } else {

            fprintf(
                stderr,
                "  --> %s:%d:%d\n",
                filename,
                line,
                column
            );
        }

    } else {

        fprintf(
            stderr,
            "  --> %d:%d\n",
            line,
            column
        );
    }

    /*
     * 3. Contexto visual: la línea de código y el
     *    subrayado ^ del token problemático.
     */
    if (source_line != NULL) {

        fprintf(stderr, "   |\n");

        if (color) {

            fprintf(
                stderr,
                "%s%2d%s | %s\n",
                REM_COLOR_BOLD,
                line,
                REM_COLOR_RESET,
                source_line
            );

        } else {

            fprintf(
                stderr,
                "%2d | %s\n",
                line,
                source_line
            );
        }

        if (column > 0) {

            fprintf(stderr, "   | ");

            for (int i = 1;
                 i < column;
                 i++) {

                fputc(' ', stderr);
            }

            int caret_length =
                length > 0 ? length : 1;

            if (color)
                fputs(accent, stderr);

            for (int i = 0;
                 i < caret_length;
                 i++) {

                fputc('^', stderr);
            }

            if (color)
                fputs(REM_COLOR_RESET, stderr);

            fputc('\n', stderr);
        }

        fprintf(stderr, "   |\n");
    }

    /*
     * 4. Detalle.
     */
    if (detail != NULL) {

        if (color) {

            fprintf(
                stderr,
                "   = %sdetalle:%s %s\n",
                REM_COLOR_CYAN,
                REM_COLOR_RESET,
                detail
            );

        } else {

            fprintf(
                stderr,
                "   = detalle: %s\n",
                detail
            );
        }
    }

    /*
     * 5. Sugerencia de arreglo.
     */
    if (suggestion != NULL) {

        if (color) {

            fprintf(
                stderr,
                "   = %ssugerencia:%s %s\n",
                REM_COLOR_YELLOW,
                REM_COLOR_RESET,
                suggestion
            );

        } else {

            fprintf(
                stderr,
                "   = sugerencia: %s\n",
                suggestion
            );
        }
    }

    fputc('\n', stderr);
}


/* =========================================================
 * IMPRIMIR ERROR
 * ========================================================= */

void error_print(const CompilerError *error)
{
    if (error == NULL)
        return;

    rem_render(
        error->severity,
        error_kind_class(error->kind),
        error->filename,
        error->source_line,
        error->location.line,
        error->location.column,
        error->length,
        error->message,
        error->detail,
        error->suggestion
    );
}


/* =========================================================
 * RENDER REM INMEDIATO (sin ErrorContext)
 * ========================================================= */

void error_render(
    const char *class_name,
    ErrorSeverity severity,
    const char *filename,
    const char *source,
    int line,
    int column,
    int length,
    const char *message,
    const char *detail,
    const char *suggestion
)
{
    if (class_name == NULL)
        class_name = "Error";

    char *source_line = NULL;

    if (source != NULL && line > 0) {

        source_line =
            rem_line_copy(source, line);
    }

    rem_render(
        severity,
        class_name,
        filename,
        source_line,
        line,
        column,
        length,
        message,
        detail,
        suggestion
    );

    free(source_line);
}


/* =========================================================
 * IMPRIMIR TODOS
 * ========================================================= */

void error_print_all(
    const ErrorContext *context
)
{
    if (context == NULL)
        return;

    for (int i = 0;
         i < context->errors.count;
         i++) {

        error_print(
            context->errors.errors[i]
        );
    }

    /*
     * Resumen.
     */
    if (context->error_count > 0 ||
        context->warning_count > 0) {

        fprintf(
            stderr,
            "E++: %d error%s, %d warning%s\n",
            context->error_count,
            context->error_count == 1
                ? ""
                : "s",
            context->warning_count,
            context->warning_count == 1
                ? ""
                : "s"
        );
    }
}


/* =========================================================
 * LÍNEA DEL CÓDIGO FUENTE
 * ========================================================= */

const char *error_get_source_line(
    const ErrorContext *context,
    int line
)
{
    if (context == NULL ||
        context->source == NULL ||
        line <= 0)
        return NULL;

    const char *start = context->source;

    int current_line = 1;

    while (*start != '\0' &&
           current_line < line) {

        if (*start == '\n')
            current_line++;

        start++;
    }

    if (current_line != line)
        return NULL;

    const char *end = start;

    while (*end != '\0' &&
           *end != '\n' &&
           *end != '\r') {

        end++;
    }

    /*
     * Esta función devuelve un puntero al source original,
     * no una copia.
     */
    return start;
}


/* =========================================================
 * LIBERAR STRING
 * ========================================================= */

void error_free_string(char *string)
{
    free(string);
}