#ifndef AST_H
#define AST_H

#include "types.h"


/* =========================================================
 * LISTAS
 * ========================================================= */

typedef struct ASTNode ASTNode;


/*
 * Lista genérica de nodos AST.
 */
typedef struct ASTNodeList {

    ASTNode **items;

    int count;
    int capacity;

} ASTNodeList;


/* =========================================================
 * TIPOS DE NODO
 * ========================================================= */

typedef enum {

    /* -----------------------------------------------------
     * Programa
     * ----------------------------------------------------- */

    AST_PROGRAM,


    /* -----------------------------------------------------
     * Declaraciones
     * ----------------------------------------------------- */

    AST_VAR_DECL,
    AST_CONST_DECL,
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,


    /* -----------------------------------------------------
     * Imports
     * ----------------------------------------------------- */

    AST_IMPORT,
    AST_FROM_IMPORT,


    /* -----------------------------------------------------
     * Bloques y sentencias
     * ----------------------------------------------------- */

    AST_BLOCK,
    AST_EXPRESSION_STMT,

    AST_RETURN,

    AST_IF,
    AST_WHILE,
    AST_FOR,

    AST_BREAK,
    AST_CONTINUE,
    AST_PASS,


    /* -----------------------------------------------------
     * Manejo de errores
     * ----------------------------------------------------- */

    AST_TRY,
    AST_CATCH,


    /* -----------------------------------------------------
     * Expresiones
     * ----------------------------------------------------- */

    AST_BINARY,
    AST_UNARY,
    AST_AWAIT,
    AST_ASSIGN,

    AST_CALL,
    AST_INDEX,
    AST_MEMBER,


    /* -----------------------------------------------------
     * Identificadores
     * ----------------------------------------------------- */

    AST_IDENTIFIER,


    /* -----------------------------------------------------
     * Literales
     * ----------------------------------------------------- */

    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_BOOLEAN_LITERAL,
    AST_NULL_LITERAL,


    /* -----------------------------------------------------
     * Colecciones
     * ----------------------------------------------------- */

    AST_LIST_LITERAL,
    AST_MAP_LITERAL,


    /* -----------------------------------------------------
     * f-string
     *
     * f"hola {nombre}"
     * ----------------------------------------------------- */

    AST_FSTRING

} ASTNodeKind;


/* =========================================================
 * OPERADORES
 * ========================================================= */

typedef enum {

    /* -----------------------------------------------------
     * Aritméticos
     * ----------------------------------------------------- */

    AST_OP_ADD,
    AST_OP_SUB,
    AST_OP_MUL,
    AST_OP_DIV,
    AST_OP_FLOOR_DIV,
    AST_OP_MOD,


    /* -----------------------------------------------------
     * Comparación
     * ----------------------------------------------------- */

    AST_OP_EQUAL,
    AST_OP_NOT_EQUAL,

    AST_OP_GREATER,
    AST_OP_LESS,

    AST_OP_GREATER_EQUAL,
    AST_OP_LESS_EQUAL,


    /* -----------------------------------------------------
     * Lógicos
     * ----------------------------------------------------- */

    AST_OP_AND,
    AST_OP_OR,


    /* -----------------------------------------------------
     * Asignación
     * ----------------------------------------------------- */

    AST_OP_ASSIGN,

    AST_OP_ADD_ASSIGN,
    AST_OP_SUB_ASSIGN,
    AST_OP_MUL_ASSIGN,
    AST_OP_DIV_ASSIGN,


    /* -----------------------------------------------------
     * Unarios
     * ----------------------------------------------------- */

    AST_OP_NEGATE,
    AST_OP_NOT,
    AST_OP_INCREMENT,
    AST_OP_DECREMENT

} ASTOperator;


/* =========================================================
 * PARÁMETROS
 * ========================================================= */

typedef struct ASTParameter {

    /*
     * Nombre del parámetro.
     *
     * Ejemplo:
     *
     * a
     */
    char *name;


    /*
     * Tipo.
     *
     * Ejemplo:
     *
     * int
     * string
     * list<int>
     */
    Type *type;


    /*
     * Indica si empieza por '_'.
     */
    int is_private;

} ASTParameter;


/*
 * Lista de parámetros.
 */
typedef struct ASTParameterList {

    ASTParameter **items;

    int count;
    int capacity;

} ASTParameterList;


/* =========================================================
 * ENTRADAS DE MAP
 * ========================================================= */

typedef struct ASTMapEntry {

    ASTNode *key;

    ASTNode *value;

} ASTMapEntry;


/*
 * Lista de entradas de un mapa.
 */
typedef struct ASTMapEntryList {

    ASTMapEntry **items;

    int count;
    int capacity;

} ASTMapEntryList;


/* =========================================================
 * SEGMENTOS DE F-STRING
 * ========================================================= */

/*
 * Un f-string es una lista ordenada de segmentos:
 *
 *     texto literal | expresión entre llaves
 *
 * Ejemplo:
 *
 *     f"hola {nombre}, tienes {edad} años"
 *
 * segmentos:
 *
 *     texto "hola ", expr nombre,
 *     texto ", tienes ", expr edad, texto " años"
 */
typedef struct ASTFStringSegment {

    /* 1 = expresión, 0 = texto literal */
    int is_expr;

    /* Texto literal (resuelto); NULL si is_expr */
    char *text;

    /* Expresión; NULL si no is_expr */
    ASTNode *expr;

} ASTFStringSegment;


/*
 * Lista de segmentos de un f-string.
 */
typedef struct ASTFStringSegmentList {

    ASTFStringSegment **items;

    int count;
    int capacity;

} ASTFStringSegmentList;


/* =========================================================
 * NODO AST
 * ========================================================= */

struct ASTNode {

    /*
     * Tipo de nodo.
     */
    ASTNodeKind kind;


    /*
     * Posición original en el código.
     *
     * Fundamental para errores.
     */
    int line;
    int column;


    /*
     * Tipo de la expresión.
     *
     * Lo rellena semantic_analyze_expression() con una copia
     * del tipo devuelto; el backend (llvm.c) lo usa para
     * generar código sin tener que recuperar el tipo por su
     * cuenta (list<T> frente a map<K, V>, tipo de elemento,
     * etiquetas del runtime).
     *
     * El AST es propietario: se destruye en ast_destroy().
     * NULL en nodos que no son expresiones o cuando el
     * análisis semántico falló.
     */
    Type *expr_type;


    /*
     * Información específica del nodo.
     */
    union {


        /* =================================================
         * PROGRAMA
         * ================================================= */

        struct {

            ASTNodeList declarations;

        } program;


        /* =================================================
         * IDENTIFICADOR
         * ================================================= */

        struct {

            char *name;

        } identifier;


        /* =================================================
         * VARIABLE
         *
         * var edad:int = 20
         * ================================================= */

        struct {

            char *name;

            Type *type;

            ASTNode *value;

            int is_private;

        } var_decl;


        /* =================================================
         * CONSTANTE
         *
         * const PI:float = 3.14
         * ================================================= */

        struct {

            char *name;

            Type *type;

            ASTNode *value;

            int is_private;

        } const_decl;


        /* =================================================
         * FUNCIÓN
         *
         * fn suma(a:int, b:int) -> int:
         *     ...
         *
         * Función externa (sin cuerpo, declarada en un .c):
         *
         * external "ruta.c" fn suma(a:int, b:int) -> int
         * ================================================= */

        struct {

            char *name;

            ASTParameterList parameters;

            Type *return_type;

            ASTNode *body;

            int is_private;

            /*
             * async fn ... (1 = función asíncrona; la
             * llamada produce task<T>).
             */
            int is_async;

            /*
             * Ruta del fichero C que implementa la función.
             * NULL en una función normal; body debe ser
             * NULL cuando esta ruta está presente.
             */
            char *external_path;

            /*
             * Propiedad del string devuelto por una función
             * externa (solo si return_type es string):
             *   0 = borrowed (default, comportamiento actual)
             *   1 = owned  (E++ copia a arena y libera el original)
             *   2 = static (sinónimo explícito de borrowed)
             */
            int external_string_owned;
            int external_string_static;

            /*
             * `external "lib.c" fn printf(fmt:string, ...) -> int`
             * Función variadic C: los argumentos extra se
             * pasan sin comprobar tipos.
             */
            int is_variadic;

        } function_decl;


        /* =================================================
         * STRUCT
         *
         * struct Punto:
         *     var x:float
         *     var y:float
         *
         * Solo datos: sin métodos ni herencia (decisión de
         * diseño 2026-10-17). Cada campo es un AST_VAR_DECL
         * sin valor inicial.
         * ================================================= */

        struct {

            char *name;

            ASTNodeList fields;

        } struct_decl;


        /* =================================================
         * BLOQUE
         * ================================================= */

        struct {

            ASTNodeList statements;

        } block;


        /* =================================================
         * EXPRESIÓN COMO SENTENCIA
         *
         * print("Hola")
         * ================================================= */

        struct {

            ASTNode *expression;

        } expression_stmt;


        /* =================================================
         * RETURN
         *
         * return resultado
         * ================================================= */

        struct {

            ASTNode *value;

        } return_stmt;


        /* =================================================
         * IF
         * ================================================= */

        struct {

            ASTNode *condition;

            ASTNode *then_branch;

            ASTNode *else_branch;

        } if_stmt;


        /* =================================================
         * WHILE
         * ================================================= */

        struct {

            ASTNode *condition;

            ASTNode *body;

        } while_stmt;


        /* =================================================
         * FOR
         *
         * for nombre in nombres:
         * ================================================= */

        struct {

            char *variable;

            ASTNode *iterable;

            ASTNode *body;

            /*
             * Tipo del elemento del iterable.
             *
             * Lo calcula el análisis semántico: el backend
             * no puede deducirlo porque los punteros LLVM
             * son opacos.
             */
            Type *element_type;

        } for_stmt;


        /* =================================================
         * BREAK
         * ================================================= */

        struct {

            int dummy;

        } break_stmt;


        /* =================================================
         * CONTINUE
         * ================================================= */

        struct {

            int dummy;

        } continue_stmt;


        /* =================================================
         * TRY
         *
         * try:
         *     ...
         *
         * catch error:
         *     ...
         * ================================================= */

        struct {

            ASTNode *try_body;

            ASTNodeList catches;

        } try_stmt;


        /* =================================================
         * CATCH
         *
         * catch error:
         *     ...
         * ================================================= */

        struct {

            char *error_name;

            Type *error_type;

            ASTNode *body;

        } catch_stmt;


        /* =================================================
         * OPERACIÓN BINARIA
         *
         * a + b
         * a == b
         * a and b
         * ================================================= */

        struct {

            ASTOperator operator;

            ASTNode *left;

            ASTNode *right;

        } binary;


        /* =================================================
         * OPERACIÓN UNARIA
         *
         * -a
         * not activo
         * ================================================= */

        struct {

            ASTOperator operator;

            ASTNode *operand;

            int is_postfix;

        } unary;


        /* =================================================
         * AWAIT
         *
         * await tarea
         * ================================================= */

        struct {

            ASTNode *operand;

        } await_expr;


        /* =================================================
         * ASIGNACIÓN
         *
         * edad = 20
         * edad += 1
         * ================================================= */

        struct {

            ASTNode *target;

            ASTOperator operator;

            ASTNode *value;

        } assignment;


        /* =================================================
         * LLAMADA
         *
         * suma(10, 20)
         * ================================================= */

        struct {

            ASTNode *callee;

            ASTNodeList arguments;

            /*
             * 1 si la semántica resolvió la llamada como una
             * función incorporada del runtime (join, upper...).
             * El codegen debe ir directo al builtin: en el
             * espacio plano de nombres LLVM una función de
             * módulo con el mismo nombre no debe eclipsarla.
             */
            int is_builtin;

            /* 1 si la llamada construye un struct. */
            int is_struct_constructor;

            /*
             * 1 si la función externa devolvió un entero FFI
             * sin signo (u8/u16/u32/u64/size): el codegen
             * debe extender con ceros a i64 (E++ int).
             */
            int ffi_return_unsigned;

            /*
             * 1 si la función externa devuelve `-> string
             * owned`: el codegen debe copiar el resultado a
             * la arena y liberar el original con free.
             */
            int ffi_string_owned;

        } call;


        /* =================================================
         * ACCESO POR ÍNDICE
         *
         * numeros[0]
         * edades["Ana"]
         * ================================================= */

        struct {

            ASTNode *object;

            ASTNode *index;

        } index;


        /* =================================================
         * ACCESO A MIEMBRO
         *
         * lista.add(...)
         * lista.len()
         * ================================================= */

        struct {

            ASTNode *object;

            char *member;

        } member;


        /* =================================================
         * INT
         * ================================================= */

        struct {

            long long value;

        } int_literal;


        /* =================================================
         * FLOAT
         * ================================================= */

        struct {

            double value;

        } float_literal;


        /* =================================================
         * TEXTO
         * ================================================= */

        struct {

            char *value;

        } string_literal;


        /* =================================================
         * CHAR
         * ================================================= */

        struct {

            char value;

        } char_literal;


        /* =================================================
         * BOOLEANO
         * ================================================= */

        struct {

            int value;

        } boolean_literal;


        /* =================================================
         * NULL
         * ================================================= */

        struct {

            int dummy;

        } null_literal;


        /* =================================================
         * LISTA
         *
         * [1, 2, 3]
         * ================================================= */

        struct {

            ASTNodeList elements;

        } list_literal;


        /* =================================================
         * MAPA
         *
         * ["Ana": 20, "Luis": 25]
         * ================================================= */

        struct {

            ASTMapEntryList entries;

        } map_literal;


        /* =================================================
         * F-STRING
         *
         * f"hola {nombre}"
         * ================================================= */

        struct {

            ASTFStringSegmentList segments;

        } fstring;


        /* =================================================
         * IMPORT
         *
         * import matematicas
         * import matematicas as math
         * ================================================= */

        struct {

            char *module;

            char *alias;

            /*
             * Declaraciones del módulo resuelto por el loader
             * (propiedad de este nodo; vacío si el módulo
             * ya estaba cargado).
             */
            ASTNodeList declarations;

        } import_decl;


        /* =================================================
         * FROM IMPORT
         *
         * from matematicas import suma
         * ================================================= */

        struct {

            char *module;

            ASTNodeList names;

            /* Declaraciones del módulo (ver import_decl) */
            ASTNodeList declarations;

        } from_import;


    } data;

};


/* =========================================================
 * CREACIÓN
 * ========================================================= */

/*
 * Crea un nodo AST.
 */
ASTNode *ast_create(
    ASTNodeKind kind
);


/*
 * Destruye recursivamente un nodo AST.
 */
void ast_destroy(
    ASTNode *node
);


/* =========================================================
 * LISTAS
 * ========================================================= */

/*
 * Inicializa una lista de nodos.
 */
void ast_node_list_init(
    ASTNodeList *list
);


/*
 * Añade un nodo a una lista.
 */
void ast_node_list_add(
    ASTNodeList *list,
    ASTNode *node
);


/*
 * Libera una lista de nodos.
 */
void ast_node_list_destroy(
    ASTNodeList *list
);


/* =========================================================
 * PARÁMETROS
 * ========================================================= */

/*
 * Crea un parámetro.
 */
ASTParameter *ast_parameter_create(
    char *name,
    Type *type
);


/*
 * Añade un parámetro.
 */
void ast_parameter_list_add(
    ASTParameterList *list,
    ASTParameter *parameter
);


/*
 * Libera los parámetros.
 */
void ast_parameter_list_destroy(
    ASTParameterList *list
);


/* =========================================================
 * MAPAS
 * ========================================================= */

/*
 * Crea una entrada de mapa.
 */
ASTMapEntry *ast_map_entry_create(
    ASTNode *key,
    ASTNode *value
);


/*
 * Añade una entrada al mapa.
 */
void ast_map_entry_list_add(
    ASTMapEntryList *list,
    ASTMapEntry *entry
);


/*
 * Libera las entradas del mapa.
 */
void ast_map_entry_list_destroy(
    ASTMapEntryList *list
);


/* =========================================================
 * SEGMENTOS DE F-STRING
 * ========================================================= */

/*
 * Crea un segmento de f-string.
 *
 * is_expr = 0 -> texto (text != NULL, expr == NULL)
 * is_expr = 1 -> expresión (expr != NULL, text == NULL)
 */
ASTFStringSegment *ast_fstring_segment_create(
    int is_expr,
    char *text,
    ASTNode *expr
);


/*
 * Añade un segmento a la lista.
 */
void ast_fstring_segment_list_add(
    ASTFStringSegmentList *list,
    ASTFStringSegment *segment
);


/*
 * Libera los segmentos.
 */
void ast_fstring_segment_list_destroy(
    ASTFStringSegmentList *list
);


/* =========================================================
 * DEBUG
 * ========================================================= */

/*
 * Imprime el AST.
 *
 * Útil para comprobar que el parser funciona
 * correctamente antes del análisis semántico.
 */
void ast_print(
    const ASTNode *node
);


/*
 * Convierte un tipo de nodo en texto.
 */
const char *ast_node_kind_name(
    ASTNodeKind kind
);


/*
 * Convierte un operador en texto.
 */
const char *ast_operator_name(
    ASTOperator operator
);

#endif