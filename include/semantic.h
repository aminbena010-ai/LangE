#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "types.h"


/* =========================================================
 * CONSTANTES
 * ========================================================= */

#define SEMANTIC_MAX_SCOPE_DEPTH 256
#define SEMANTIC_INITIAL_SYMBOL_CAPACITY 32
#define SEMANTIC_INITIAL_ERROR_CAPACITY 16


/* =========================================================
 * TIPOS DE SÍMBOLO
 * ========================================================= */

typedef enum {

    /* Variables */
    SYMBOL_VARIABLE,

    /* Constantes */
    SYMBOL_CONSTANT,

    /* Funciones */
    SYMBOL_FUNCTION,

    /* Tipos de struct */
    SYMBOL_STRUCT,

    /* Parámetros de función */
    SYMBOL_PARAMETER

} SymbolKind;


/* =========================================================
 * SÍMBOLO
 * ========================================================= */

typedef struct SemanticSymbol {

    /* Nombre del símbolo */
    char *name;

    /* Tipo */
    Type *type;

    /* Tipo de símbolo */
    SymbolKind kind;

    /* Visibilidad */
    int is_private;

    /* Constante */
    int is_const;

    /* Inicializado */
    int is_initialized;

    /* Usado al menos una vez (para avisos) */
    int is_used;

    /* Declarado en */
    int line;
    int column;

    /* Para funciones */
    struct SemanticFunction *function;

    /* Para SYMBOL_STRUCT; el AST es propiedad del programa. */
    ASTNode *struct_decl;

} SemanticSymbol;


/* =========================================================
 * LISTA DE SÍMBOLOS
 * ========================================================= */

typedef struct SemanticSymbolTable {

    SemanticSymbol **symbols;

    int count;
    int capacity;

} SemanticSymbolTable;


/* =========================================================
 * SCOPE
 * ========================================================= */

typedef struct SemanticScope {

    SemanticSymbolTable symbols;

    struct SemanticScope *parent;

    int depth;

} SemanticScope;


/* =========================================================
 * MÓDULOS
 * ========================================================= */

typedef struct SemanticModule SemanticModule;

typedef struct SemanticAlias {

    /* Nombre visible (alias o nombre del módulo) */
    char *name;

    /* Módulo apuntado */
    SemanticModule *module;

    /* Scope donde se declaró (controla la visibilidad) */
    SemanticScope *owner;

} SemanticAlias;

struct SemanticModule {

    /* Nombre del módulo (archivo .ro sin extensión) */
    char *name;

    /* Declaraciones del módulo (propiedad del AST) */
    ASTNodeList *declarations;

    /* Scope de funciones del módulo (sin parent) */
    SemanticScope *scope;

    /*
     * Nombres que este módulo exportó vía from-import
     * (azúcar sintáctico sobre el mismo símbolo).
     */
    char **exported;
    int exported_count;

};


/* =========================================================
 * PARÁMETRO DE FUNCIÓN
 * ========================================================= */

typedef struct SemanticParameter {

    char *name;

    Type *type;

    int is_private;

} SemanticParameter;


/* =========================================================
 * INFORMACIÓN DE FUNCIÓN
 * ========================================================= */

typedef struct SemanticFunction {

    char *name;

    Type *return_type;

    SemanticParameter **parameters;

    int parameter_count;
    int parameter_capacity;

    /* Función privada */
    int is_private;

    /* Función async (§52) */
    int is_async;

    /*
     * `-> string owned` en función externa: el codegen
     * copia el resultado a la arena y libera el original.
     */
    int external_string_owned;

    /*
     * `external "lib.c" fn printf(fmt:string, ...) -> int`
     * Función variadic C.
     */
    int is_variadic;

} SemanticFunction;


/* =========================================================
 * ERROR SEMÁNTICO
 * ========================================================= */

typedef enum {

    /* General */
    SEMANTIC_ERROR_UNKNOWN,
    SEMANTIC_ERROR_UNDEFINED_NAME,
    SEMANTIC_ERROR_DUPLICATE_NAME,

    /* Tipos */
    SEMANTIC_ERROR_TYPE_MISMATCH,
    SEMANTIC_ERROR_INVALID_TYPE,
    SEMANTIC_ERROR_INVALID_OPERATION,

    /* Variables */
    SEMANTIC_ERROR_VARIABLE_NOT_INITIALIZED,
    SEMANTIC_ERROR_ASSIGN_TO_CONST,
    SEMANTIC_ERROR_INVALID_ASSIGNMENT,

    /* Funciones */
    SEMANTIC_ERROR_FUNCTION_NOT_FOUND,
    SEMANTIC_ERROR_ARGUMENT_COUNT,
    SEMANTIC_ERROR_ARGUMENT_TYPE,
    SEMANTIC_ERROR_RETURN_TYPE,

    /* Colecciones */
    SEMANTIC_ERROR_INVALID_INDEX,
    SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
    SEMANTIC_ERROR_INVALID_KEY_TYPE,

    /* Control de flujo */
    SEMANTIC_ERROR_INVALID_BREAK,
    SEMANTIC_ERROR_INVALID_CONTINUE,
    SEMANTIC_ERROR_RETURN_OUTSIDE_FUNCTION,

    /* Visibilidad */
    SEMANTIC_ERROR_PRIVATE_ACCESS,

    /* Null */
    SEMANTIC_ERROR_NULL_NOT_ALLOWED,

    /* Import */
    SEMANTIC_ERROR_MODULE_NOT_FOUND,
    SEMANTIC_ERROR_IMPORT_NOT_FOUND,

    /* Avisos */
    SEMANTIC_WARNING_UNUSED_VARIABLE

} SemanticErrorKind;


/* =========================================================
 * ERROR
 * ========================================================= */

typedef struct SemanticError {

    SemanticErrorKind kind;

    char *message;

    /* Sugerencia de arreglo (sistema REM); puede ser NULL */
    char *suggestion;

    int line;
    int column;

    /* 1 si es un aviso y no un error */
    int is_warning;

} SemanticError;


/* =========================================================
 * LISTA DE ERRORES
 * ========================================================= */

typedef struct SemanticErrorList {

    SemanticError **errors;

    int count;
    int capacity;

} SemanticErrorList;


/* =========================================================
 * CONTEXTO SEMÁNTICO
 * ========================================================= */

typedef struct SemanticContext {

    /* Scope global */
    SemanticScope *global_scope;

    /* Scope actual */
    SemanticScope *current_scope;

    /* Función actual */
    SemanticFunction *current_function;

    /* Profundidad de scopes */
    int scope_depth;

    /* Estamos dentro de un bucle */
    int loop_depth;

    /* Estamos dentro de una función */
    int function_depth;

    /*
     * Cuerpos de try activos (decisión 2026-10-08):
     * return/break/continue no se permiten con valor > 0
     * (claude.md §15).
     */
    int try_depth;

    /* Errores */
    SemanticErrorList errors;

    /* Número total de errores */
    int error_count;

    /* El análisis encontró errores */
    int had_error;

    /* Módulos importados */
    SemanticModule **modules;
    int module_count;
    int module_capacity;

    /* Aliases de import (alias as módulo) */
    SemanticAlias *aliases;
    int alias_count;
    int alias_capacity;

} SemanticContext;


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

/*
 * Crea el contexto semántico.
 */
SemanticContext *semantic_create(void);


/*
 * Destruye el contexto semántico.
 */
void semantic_destroy(SemanticContext *context);


/* =========================================================
 * ANÁLISIS PRINCIPAL
 * ========================================================= */

/*
 * Analiza un programa completo.
 *
 * Devuelve 1 si el programa es válido.
 * Devuelve 0 si existen errores.
 */
int semantic_analyze(
    SemanticContext *context,
    ASTNode *program
);


/*
 * Analiza una declaración.
 */
int semantic_analyze_declaration(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza una sentencia.
 */
int semantic_analyze_statement(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza una expresión.
 *
 * Devuelve el tipo resultante de la expresión.
 */
Type *semantic_analyze_expression(
    SemanticContext *context,
    ASTNode *node
);


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

/*
 * Analiza:
 *
 * var nombre:tipo = valor
 */
int semantic_analyze_var(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza:
 *
 * const nombre:tipo = valor
 */
int semantic_analyze_const(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza una función.
 */
int semantic_analyze_function(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza imports.
 */
int semantic_analyze_import(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Analiza from ... import ...
 */
int semantic_analyze_from_import(
    SemanticContext *context,
    ASTNode *node
);


/* =========================================================
 * SENTENCIAS
 * ========================================================= */

/*
 * if / else
 */
int semantic_analyze_if(
    SemanticContext *context,
    ASTNode *node
);


/*
 * while
 */
int semantic_analyze_while(
    SemanticContext *context,
    ASTNode *node
);


/*
 * for
 */
int semantic_analyze_for(
    SemanticContext *context,
    ASTNode *node
);


/*
 * return
 */
int semantic_analyze_return(
    SemanticContext *context,
    ASTNode *node
);


/*
 * try / catch
 */
int semantic_analyze_try(
    SemanticContext *context,
    ASTNode *node
);


/*
 * break
 */
int semantic_analyze_break(
    SemanticContext *context,
    ASTNode *node
);


/*
 * continue
 */
int semantic_analyze_continue(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Bloque.
 */
int semantic_analyze_block(
    SemanticContext *context,
    ASTNode *node
);


/* =========================================================
 * EXPRESIONES
 * ========================================================= */

/*
 * Identificador.
 */
Type *semantic_analyze_identifier(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Operación binaria.
 */
Type *semantic_analyze_binary(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Operación unaria.
 */
Type *semantic_analyze_unary(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Asignación.
 */
Type *semantic_analyze_assignment(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Llamada a función.
 */
Type *semantic_analyze_call(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Acceso mediante índice.
 *
 * lista[0]
 * mapa["clave"]
 */
Type *semantic_analyze_index(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Acceso a miembro.
 *
 * lista.add()
 * lista.len()
 */
Type *semantic_analyze_member(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Lista.
 */
Type *semantic_analyze_list(
    SemanticContext *context,
    ASTNode *node
);


/*
 * Mapa.
 */
Type *semantic_analyze_map(
    SemanticContext *context,
    ASTNode *node
);


/* =========================================================
 * TIPOS
 * ========================================================= */

/*
 * Comprueba si dos tipos son iguales.
 */
int semantic_types_equal(
    const Type *a,
    const Type *b
);


/*
 * Comprueba si un valor puede asignarse a un tipo.
 *
 * Ejemplo:
 *
 * int <- int       TRUE
 * int <- string    FALSE
 */
int semantic_type_compatible(
    const Type *expected,
    const Type *actual
);


/*
 * Comprueba una operación binaria.
 */
Type *semantic_binary_result_type(
    ASTOperator operator,
    const Type *left,
    const Type *right
);


/*
 * Comprueba una operación unaria.
 */
Type *semantic_unary_result_type(
    ASTOperator operator,
    const Type *operand
);


/*
 * Comprueba si un tipo es bool.
 */
int semantic_is_boolean(
    const Type *type
);


/*
 * Comprueba si un tipo es numérico.
 */
int semantic_is_numeric(
    const Type *type
);


/*
 * Comprueba si un tipo admite null.
 */
int semantic_is_nullable(
    const Type *type
);


/* =========================================================
 * SÍMBOLOS
 * ========================================================= */

/*
 * Declara un símbolo en el scope actual.
 */
int semantic_declare_symbol(
    SemanticContext *context,
    SemanticSymbol *symbol
);


/*
 * Busca un símbolo empezando por el scope actual.
 */
SemanticSymbol *semantic_lookup_symbol(
    SemanticContext *context,
    const char *name
);


/*
 * Busca solamente en el scope actual.
 */
SemanticSymbol *semantic_lookup_current_scope(
    SemanticContext *context,
    const char *name
);


/*
 * Comprueba si un nombre ya está declarado.
 */
int semantic_symbol_exists(
    SemanticContext *context,
    const char *name
);


/* =========================================================
 * SCOPES
 * ========================================================= */

/*
 * Entra en un nuevo scope.
 */
int semantic_enter_scope(
    SemanticContext *context
);


/*
 * Sale del scope actual.
 */
int semantic_leave_scope(
    SemanticContext *context
);


/*
 * Obtiene el scope actual.
 */
SemanticScope *semantic_current_scope(
    SemanticContext *context
);


/* =========================================================
 * FUNCIONES
 * ========================================================= */

/*
 * Busca una función.
 */
SemanticFunction *semantic_lookup_function(
    SemanticContext *context,
    const char *name
);


/*
 * Comprueba los argumentos de una llamada.
 */
int semantic_check_arguments(
    SemanticContext *context,
    SemanticFunction *function,
    ASTNodeList *arguments
);


/*
 * Comprueba el tipo de retorno.
 */
int semantic_check_return(
    SemanticContext *context,
    Type *actual_type
);


/* =========================================================
 * CONTROL DE FLUJO
 * ========================================================= */

/*
 * Entra en un bucle.
 */
void semantic_enter_loop(
    SemanticContext *context
);


/*
 * Sale de un bucle.
 */
void semantic_leave_loop(
    SemanticContext *context
);


/*
 * Comprueba si estamos dentro de un bucle.
 */
int semantic_inside_loop(
    SemanticContext *context
);


/*
 * Comprueba si estamos dentro de una función.
 */
int semantic_inside_function(
    SemanticContext *context
);


/* =========================================================
 * VISIBILIDAD
 * ========================================================= */

/*
 * Comprueba si un símbolo privado puede utilizarse
 * desde el contexto actual.
 */
int semantic_check_visibility(
    SemanticContext *context,
    SemanticSymbol *symbol
);


/*
 * Comprueba si un nombre comienza por '_'.
 */
int semantic_is_private_name(
    const char *name
);


/* =========================================================
 * ERRORES
 * ========================================================= */

/*
 * Registra un error semántico.
 */
void semantic_error(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message
);


/*
 * Registra un error con sugerencia de arreglo (REM).
 */
void semantic_error_hint(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message,
    const char *suggestion
);


/*
 * Devuelve el número de errores.
 */
int semantic_error_count(
    const SemanticContext *context
);


/*
 * Devuelve 1 si hubo errores.
 */
int semantic_has_error(
    const SemanticContext *context
);


/*
 * Obtiene un error por índice.
 */
const SemanticError *semantic_get_error(
    const SemanticContext *context,
    int index
);


/*
 * Limpia todos los errores.
 */
void semantic_clear_errors(
    SemanticContext *context
);


/*
 * Clase estilo Python del error semántico
 * (SyntaxError, TypeError, NameError, ...).
 */
const char *semantic_error_class_name(
    SemanticErrorKind kind
);


/* =========================================================
 * UTILIDADES DE TIPOS
 * ========================================================= */

/*
 * Crea un tipo básico.
 */
Type *semantic_create_basic_type(
    TypeKind kind
);


/*
 * Crea list<T>.
 */
Type *semantic_create_list_type(
    Type *element_type
);


/*
 * Crea map<K, V>.
 */
Type *semantic_create_map_type(
    Type *key_type,
    Type *value_type
);


/*
 * Libera un tipo y sus tipos internos.
 */
void semantic_destroy_type(
    Type *type
);

#endif