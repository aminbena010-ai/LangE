#ifndef LLVM_H
#define LLVM_H

#include "ast.h"
#include "types.h"
#include "error.h"

/*
 * API oficial de LLVM para C.
 */
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>


/* =========================================================
 * CONFIGURACIÓN DEL BACKEND
 * ========================================================= */

typedef enum {

    LLVM_TARGET_NATIVE,

    LLVM_TARGET_X86_64_WINDOWS,
    LLVM_TARGET_X86_64_LINUX,

    LLVM_TARGET_ARM64_WINDOWS,
    LLVM_TARGET_ARM64_LINUX

} LLVMTarget;


/* =========================================================
 * FORMATO DE SALIDA
 * ========================================================= */

typedef enum {

    LLVM_OUTPUT_IR,

    LLVM_OUTPUT_LL,

    LLVM_OUTPUT_ASSEMBLY,

    LLVM_OUTPUT_OBJECT,

    LLVM_OUTPUT_EXECUTABLE

} LLVMOutputType;


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

typedef struct LLVMConfig {

    /*
     * Nombre del módulo.
     */
    const char *module_name;


    /*
     * Triple de LLVM.
     *
     * Si es NULL, se utiliza el target
     * seleccionado.
     */
    const char *target_triple;


    /*
     * Arquitectura / sistema objetivo.
     */
    LLVMTarget target;


    /*
     * Nivel de optimización.
     *
     * 0 = sin optimización
     * 1 = básica
     * 2 = normal
     * 3 = máxima
     */
    int optimization_level;


    /*
     * Generar información de debug.
     */
    int debug_info;


    /*
     * Generar código PIC.
     */
    int position_independent;


    /*
     * Tipo de salida.
     */
    LLVMOutputType output_type;

} LLVMConfig;


/* =========================================================
 * SCOPES DE VARIABLES
 *
 * Tabla de símbolos que acompaña a la búsqueda de allocas
 * para resolver correctamente el sombreado de variables.
 * ========================================================= */

#define LLVM_MAX_SCOPES 64
#define LLVM_MAX_SCOPE_VARIABLES 128


typedef struct LLVMScopeVariable {

    /*
     * Nombre del identificador.
     *
     * El puntero apunta al AST: el AST vive más que el
     * compilador LLVM.
     */
    const char *name;

    /*
     * Alloca de la variable.
     */
    LLVMValueRef variable;

} LLVMScopeVariable;


typedef struct LLVMScope {

    LLVMScopeVariable variables[LLVM_MAX_SCOPE_VARIABLES];

    int count;

} LLVMScope;

typedef struct LLVMStructInfo {
    ASTNode *declaration;
    LLVMTypeRef type;
} LLVMStructInfo;


/* =========================================================
 * CONTEXTO LLVM
 * ========================================================= */

typedef struct LLVMCompiler {

    /*
     * Contexto global de LLVM.
     */
    LLVMContextRef context;


    /*
     * Módulo LLVM.
     */
    LLVMModuleRef module;


    /*
     * Builder utilizado para construir instrucciones.
     */
    LLVMBuilderRef builder;


    /*
     * Target machine.
     */
    LLVMTargetMachineRef target_machine;


    /*
     * Target seleccionado.
     */
    LLVMTargetRef target;


    /*
     * Triple objetivo.
     */
    char *target_triple;


    /*
     * Configuración.
     */
    LLVMConfig config;


    /*
     * Sistema de errores.
     */
    ErrorContext *errors;


    /*
     * Indica si la generación falló.
     */
    int had_error;


    /*
     * Pila de contextos de bucle para break/continue.
     * loop_depth indica el número de bucles anidados.
     */
    LLVMBasicBlockRef loop_break_stack[64];
    LLVMBasicBlockRef loop_continue_stack[64];
    int loop_depth;


    /*
     * Pila de ámbitos de variables.
     * scope_count indica el número de ámbitos abiertos.
     */
    LLVMScope scopes[LLVM_MAX_SCOPES];
    int scope_count;

    LLVMStructInfo *structs;
    int struct_count;
    int struct_capacity;

} LLVMCompiler;


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

/*
 * Crea un compilador LLVM.
 */
LLVMCompiler *llvm_create(
    const LLVMConfig *config,
    ErrorContext *errors
);


/*
 * Destruye el compilador LLVM.
 */
void llvm_destroy(
    LLVMCompiler *compiler
);


/*
 * Inicializa todos los targets necesarios.
 */
int llvm_initialize(void);


/*
 * Libera los recursos globales de LLVM.
 */
void llvm_shutdown(void);


/* =========================================================
 * GENERACIÓN PRINCIPAL
 * ========================================================= */

/*
 * Convierte un AST completo en LLVM IR.
 */
int llvm_generate(
    LLVMCompiler *compiler,
    ASTNode *program
);


/*
 * Genera una declaración.
 */
int llvm_generate_declaration(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Genera una sentencia.
 */
LLVMValueRef llvm_generate_statement(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Genera una expresión.
 */
LLVMValueRef llvm_generate_expression(
    LLVMCompiler *compiler,
    ASTNode *node
);


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

/*
 * Genera:
 *
 * var nombre:int = valor
 */
int llvm_generate_var(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Genera:
 *
 * const PI:float = 3.14
 */
int llvm_generate_const(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Genera una función.
 *
 * fn suma(a:int, b:int) -> int:
 *     return a + b
 */
LLVMValueRef llvm_generate_function(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Genera imports.
 */
int llvm_generate_import(
    LLVMCompiler *compiler,
    ASTNode *node
);


/* =========================================================
 * SENTENCIAS
 * ========================================================= */

/*
 * return
 */
LLVMValueRef llvm_generate_return(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * if / else
 */
LLVMValueRef llvm_generate_if(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * while
 */
LLVMValueRef llvm_generate_while(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * for
 */
LLVMValueRef llvm_generate_for(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * break
 */
LLVMValueRef llvm_generate_break(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * continue
 */
LLVMValueRef llvm_generate_continue(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * try / catch
 */
LLVMValueRef llvm_generate_try(
    LLVMCompiler *compiler,
    ASTNode *node
);


/* =========================================================
 * EXPRESIONES
 * ========================================================= */

/*
 * Identificador.
 */
LLVMValueRef llvm_generate_identifier(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Operación binaria.
 */
LLVMValueRef llvm_generate_binary(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Operación unaria.
 */
LLVMValueRef llvm_generate_unary(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Asignación.
 */
LLVMValueRef llvm_generate_assignment(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Llamada a función.
 */
LLVMValueRef llvm_generate_call(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Acceso mediante índice.
 */
LLVMValueRef llvm_generate_index(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * Acceso a miembro.
 */
LLVMValueRef llvm_generate_member(
    LLVMCompiler *compiler,
    ASTNode *node
);


/* =========================================================
 * LITERALES
 * ========================================================= */

/*
 * int
 */
LLVMValueRef llvm_generate_int(
    LLVMCompiler *compiler,
    long long value
);


/*
 * float
 */
LLVMValueRef llvm_generate_float(
    LLVMCompiler *compiler,
    double value
);


/*
 * string
 */
LLVMValueRef llvm_generate_string(
    LLVMCompiler *compiler,
    const char *value
);


/*
 * char
 */
LLVMValueRef llvm_generate_char(
    LLVMCompiler *compiler,
    char value
);


/*
 * bool
 */
LLVMValueRef llvm_generate_boolean(
    LLVMCompiler *compiler,
    int value
);


/*
 * null
 */
LLVMValueRef llvm_generate_null(
    LLVMCompiler *compiler
);


/* =========================================================
 * COLECCIONES
 * ========================================================= */

/*
 * list<T>
 */
LLVMValueRef llvm_generate_list(
    LLVMCompiler *compiler,
    ASTNode *node
);


/*
 * map<K, V>
 */
LLVMValueRef llvm_generate_map(
    LLVMCompiler *compiler,
    ASTNode *node
);


/* =========================================================
 * TIPOS
 * ========================================================= */

/*
 * Convierte un Type de nuestro lenguaje
 * al LLVMTypeRef correspondiente.
 */
LLVMTypeRef llvm_type(
    LLVMCompiler *compiler,
    const Type *type
);


/*
 * Tipos básicos.
 */
LLVMTypeRef llvm_int_type(
    LLVMCompiler *compiler
);

LLVMTypeRef llvm_float_type(
    LLVMCompiler *compiler
);

LLVMTypeRef llvm_text_type(
    LLVMCompiler *compiler
);

LLVMTypeRef llvm_boolean_type(
    LLVMCompiler *compiler
);

LLVMTypeRef llvm_char_type(
    LLVMCompiler *compiler
);


/*
 * list<T>
 */
LLVMTypeRef llvm_list_type(
    LLVMCompiler *compiler,
    const Type *element_type
);


/*
 * map<K, V>
 */
LLVMTypeRef llvm_map_type(
    LLVMCompiler *compiler,
    const Type *key_type,
    const Type *value_type
);


/* =========================================================
 * FUNCIONES
 * ========================================================= */

/*
 * Busca una función LLVM por nombre.
 */
LLVMValueRef llvm_find_function(
    LLVMCompiler *compiler,
    const char *name
);


/*
 * Crea una función LLVM.
 */
LLVMValueRef llvm_create_function(
    LLVMCompiler *compiler,
    const char *name,
    const Type *return_type,
    ASTParameterList *parameters,
    int is_variadic,
    int is_async
);


/*
 * Crea un bloque básico.
 */
LLVMBasicBlockRef llvm_create_block(
    LLVMCompiler *compiler,
    LLVMValueRef function,
    const char *name
);


/* =========================================================
 * VARIABLES
 * ========================================================= */

/*
 * Crea almacenamiento para una variable.
 */
LLVMValueRef llvm_create_variable(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef type
);


/*
 * Busca una variable LLVM.
 */
LLVMValueRef llvm_find_variable(
    LLVMCompiler *compiler,
    const char *name
);


/*
 * Carga el valor de una variable.
 */
LLVMValueRef llvm_load_variable(
    LLVMCompiler *compiler,
    LLVMValueRef variable
);


/*
 * Guarda un valor en una variable.
 */
int llvm_store_variable(
    LLVMCompiler *compiler,
    LLVMValueRef variable,
    LLVMValueRef value
);


/* =========================================================
 * OPERADORES
 * ========================================================= */

/*
 * Genera un operador binario.
 *
 * `type` es el tipo semántico del resultado (para
 * distinguir `list + list` de `string + string`, que en IR
 * son punteros iguales); puede ser NULL.
 */
LLVMValueRef llvm_build_binary(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef left,
    LLVMValueRef right,
    const Type *type
);


/*
 * Genera un operador unario.
 */
LLVMValueRef llvm_build_unary(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef operand
);


/*
 * Genera una comparación.
 */
LLVMValueRef llvm_build_comparison(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef left,
    LLVMValueRef right
);


/* =========================================================
 * OPTIMIZACIÓN
 * ========================================================= */

/*
 * Ejecuta las optimizaciones configuradas.
 */
int llvm_optimize(
    LLVMCompiler *compiler
);


/* =========================================================
 * VERIFICACIÓN
 * ========================================================= */

/*
 * Verifica que el módulo LLVM sea válido.
 */
int llvm_verify(
    LLVMCompiler *compiler
);


/*
 * Imprime el IR por consola.
 */
void llvm_dump(
    LLVMCompiler *compiler
);


/*
 * Devuelve el IR como texto.
 *
 * La memoria devuelta debe liberarse con
 * LLVMDisposeMessage().
 */
char *llvm_get_ir(
    LLVMCompiler *compiler
);


/* =========================================================
 * GENERACIÓN DE ARCHIVOS
 * ========================================================= */

/*
 * Escribe LLVM IR.
 */
int llvm_write_ir(
    LLVMCompiler *compiler,
    const char *filename
);


/*
 * Genera assembly.
 */
int llvm_write_assembly(
    LLVMCompiler *compiler,
    const char *filename
);


/*
 * Genera object file.
 */
int llvm_write_object(
    LLVMCompiler *compiler,
    const char *filename
);


/*
 * Genera el ejecutable final.
 *
 * El enlazado se realizará posteriormente.
 */
int llvm_write_executable(
    LLVMCompiler *compiler,
    const char *filename
);


/* =========================================================
 * TARGET
 * ========================================================= */

/*
 * Establece el target.
 */
int llvm_set_target(
    LLVMCompiler *compiler,
    LLVMTarget target
);


/*
 * Establece un LLVM triple manualmente.
 */
int llvm_set_target_triple(
    LLVMCompiler *compiler,
    const char *triple
);


/*
 * Obtiene el triple actual.
 */
const char *llvm_get_target_triple(
    LLVMCompiler *compiler
);


/*
 * Obtiene el nombre del target.
 */
const char *llvm_target_name(
    LLVMTarget target
);


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

/*
 * Configuración por defecto.
 */
LLVMConfig llvm_default_config(void);


/*
 * Configura el nivel de optimización.
 */
void llvm_set_optimization_level(
    LLVMCompiler *compiler,
    int level
);


/*
 * Activa/desactiva debug info.
 */
void llvm_set_debug_info(
    LLVMCompiler *compiler,
    int enabled
);


/* =========================================================
 * ERRORES
 * ========================================================= */

/*
 * Comprueba si LLVM encontró un error.
 */
int llvm_has_error(
    const LLVMCompiler *compiler
);

#endif