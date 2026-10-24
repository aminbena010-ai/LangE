#ifndef ERROR_H
#define ERROR_H


/* =========================================================
 * UBICACIÓN
 * ========================================================= */

typedef struct ErrorLocation {

    int line;
    int column;
    int offset;

} ErrorLocation;


/* =========================================================
 * SEVERIDAD
 * ========================================================= */

typedef enum ErrorSeverity {

    ERROR_INFO,
    ERROR_WARNING,
    ERROR_ERROR,
    ERROR_FATAL

} ErrorSeverity;


/* =========================================================
 * FASE
 * ========================================================= */

typedef enum ErrorPhase {

    ERROR_PHASE_LEXER,
    ERROR_PHASE_PARSER,
    ERROR_PHASE_SEMANTIC,
    ERROR_PHASE_CODEGEN,
    ERROR_PHASE_INTERNAL

} ErrorPhase;


/* =========================================================
 * TIPO DE ERROR
 * ========================================================= */

typedef enum ErrorKind {

    ERROR_NONE,

    /* -------------------------------------------------
     * LEXER
     * ------------------------------------------------- */

    ERROR_LEX_INVALID_CHARACTER,
    ERROR_LEX_INVALID_NUMBER,
    ERROR_LEX_INVALID_FLOAT,
    ERROR_LEX_UNTERMINATED_STRING,
    ERROR_LEX_UNTERMINATED_CHAR,
    ERROR_LEX_INVALID_ESCAPE,
    ERROR_LEX_INVALID_IDENTIFIER,
    ERROR_LEX_INVALID_INDENTATION,
    ERROR_LEX_UNEXPECTED_DEDENT,
    ERROR_LEX_INVALID_OPERATOR,
    ERROR_LEX_UNTERMINATED_COMMENT,
    ERROR_LEX_UNTERMINATED_FSTRING,
    ERROR_LEX_INVALID_FSTRING,

    /* -------------------------------------------------
     * PARSER
     * ------------------------------------------------- */

    ERROR_PARSE_UNEXPECTED_TOKEN,
    ERROR_PARSE_EXPECTED_TOKEN,
    ERROR_PARSE_EXPECTED_IDENTIFIER,
    ERROR_PARSE_EXPECTED_TYPE,
    ERROR_PARSE_EXPECTED_EXPRESSION,
    ERROR_PARSE_EXPECTED_STATEMENT,
    ERROR_PARSE_EXPECTED_COLON,
    ERROR_PARSE_EXPECTED_NEWLINE,
    ERROR_PARSE_EXPECTED_INDENT,
    ERROR_PARSE_EXPECTED_DEDENT,
    ERROR_PARSE_INVALID_DECLARATION,
    ERROR_PARSE_INVALID_FUNCTION,
    ERROR_PARSE_INVALID_PARAMETER,
    ERROR_PARSE_INVALID_IMPORT,
    ERROR_PARSE_INVALID_EXPRESSION,
    ERROR_PARSE_INVALID_ASSIGNMENT,
    ERROR_PARSE_MISSING_RETURN_TYPE,
    ERROR_PARSE_INVALID_TYPE,

    /* -------------------------------------------------
     * SEMÁNTICA
     * ------------------------------------------------- */

    ERROR_SEM_UNDEFINED_NAME,
    ERROR_SEM_DUPLICATE_NAME,
    ERROR_SEM_TYPE_MISMATCH,
    ERROR_SEM_INVALID_TYPE,
    ERROR_SEM_INVALID_OPERATION,
    ERROR_SEM_VARIABLE_NOT_INITIALIZED,
    ERROR_SEM_ASSIGN_TO_CONST,
    ERROR_SEM_INVALID_ASSIGNMENT,
    ERROR_SEM_FUNCTION_NOT_FOUND,
    ERROR_SEM_ARGUMENT_COUNT,
    ERROR_SEM_ARGUMENT_TYPE,
    ERROR_SEM_RETURN_TYPE,
    ERROR_SEM_INVALID_INDEX,
    ERROR_SEM_INVALID_ELEMENT_TYPE,
    ERROR_SEM_INVALID_KEY_TYPE,
    ERROR_SEM_INVALID_BREAK,
    ERROR_SEM_INVALID_CONTINUE,
    ERROR_SEM_RETURN_OUTSIDE_FUNCTION,
    ERROR_SEM_PRIVATE_ACCESS,
    ERROR_SEM_NULL_NOT_ALLOWED,
    ERROR_SEM_MODULE_NOT_FOUND,
    ERROR_SEM_IMPORT_NOT_FOUND,

    /* -------------------------------------------------
     * CODEGEN
     * ------------------------------------------------- */

    ERROR_CODEGEN_INVALID_AST,
    ERROR_CODEGEN_INVALID_TYPE,
    ERROR_CODEGEN_INVALID_OPERATION,
    ERROR_CODEGEN_FUNCTION_ERROR,
    ERROR_CODEGEN_VARIABLE_ERROR,
    ERROR_CODEGEN_LLVM_ERROR,
    ERROR_CODEGEN_TARGET_ERROR,

    /* -------------------------------------------------
     * INTERNAL
     * ------------------------------------------------- */

    ERROR_INTERNAL_NULL_POINTER,
    ERROR_INTERNAL_OUT_OF_MEMORY,
    ERROR_INTERNAL_INVALID_STATE,
    ERROR_INTERNAL_UNIMPLEMENTED

} ErrorKind;


/* =========================================================
 * ERROR DE COMPILADOR
 * ========================================================= */

typedef struct CompilerError {

    ErrorKind kind;
    ErrorSeverity severity;
    ErrorPhase phase;

    char *message;
    char *filename;

    ErrorLocation location;

    char *source_line;
    int length;

    char *detail;
    char *suggestion;

} CompilerError;


/* =========================================================
 * LISTA DE ERRORES
 * ========================================================= */

typedef struct ErrorList {

    CompilerError **errors;

    int count;
    int capacity;

} ErrorList;


/* =========================================================
 * CONTEXTO DE ERRORES
 * ========================================================= */

typedef struct ErrorContext {

    const char *filename;
    const char *source;

    ErrorList errors;

    int max_errors;

    int error_count;
    int warning_count;
    int has_fatal_error;

    /*
     * Si está activo, las advertencias cuentan como
     * errores: error_has_errors() también devuelve 1
     * cuando solo hay advertencias (rivo.toml:
     * advertencias_como_errores).
     */
    int warnings_as_errors;

} ErrorContext;


/* =========================================================
 * CONTEXTO
 * ========================================================= */

ErrorContext *error_context_create(
    const char *filename,
    const char *source
);

void error_context_destroy(ErrorContext *context);

void error_context_clear(ErrorContext *context);


/* =========================================================
 * CREACIÓN DE ERRORES
 * ========================================================= */

CompilerError *error_create(
    ErrorKind kind,
    ErrorSeverity severity,
    ErrorPhase phase,
    ErrorLocation location,
    const char *message
);

void error_destroy(CompilerError *error);


/* =========================================================
 * REGISTRO DE ERRORES
 * ========================================================= */

void error_report(
    ErrorContext *context,
    CompilerError *error
);

void error_report_simple(
    ErrorContext *context,
    ErrorKind kind,
    ErrorPhase phase,
    int line,
    int column,
    const char *message
);

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
);


/* =========================================================
 * CONSULTAS
 * ========================================================= */

int error_count(const ErrorContext *context);

int error_warning_count(const ErrorContext *context);

int error_has_errors(const ErrorContext *context);

int error_has_fatal(const ErrorContext *context);

const CompilerError *error_get(
    const ErrorContext *context,
    int index
);


/* =========================================================
 * LÍMITE DE ERRORES
 * ========================================================= */

void error_set_max_errors(
    ErrorContext *context,
    int max_errors
);

void error_set_warnings_as_errors(
    ErrorContext *context,
    int enabled
);

int error_limit_reached(
    const ErrorContext *context
);


/* =========================================================
 * NOMBRES
 *
 * error_kind_class devuelve la clase estilo Python del
 * error (SyntaxError, TypeError, NameError, ...) usada en
 * los diagnosticos impresos.
 * ========================================================= */

const char *error_kind_class(ErrorKind kind);

const char *error_phase_name(ErrorPhase phase);

const char *error_severity_name(ErrorSeverity severity);


/* =========================================================
 * IMPRESIÓN
 * ========================================================= */

void error_print(const CompilerError *error);

void error_print_all(const ErrorContext *context);


/* =========================================================
 * RENDER REM (impresión inmediata)
 *
 * Renderiza un diagnostico completo de una vez (cabecera,
 * ubicación, contexto visual con ^, detalle y sugerencia)
 * sin necesidad de un ErrorContext. Lo usan lexer, parser
 * y el vuelco semántico de main para que todos los errores
 * se pinten igual.
 *
 * Los colores ANSI solo se emiten si stderr es un
 * terminal (isatty) y NO_COLOR no está definido; en
 * pipes/ficheros la salida es texto plano y el formato
 * base se mantiene identico.
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
);


/* =========================================================
 * CÓDIGO FUENTE
 * ========================================================= */

const char *error_get_source_line(
    const ErrorContext *context,
    int line
);

void error_free_string(char *string);


#endif /* ERROR_H */
