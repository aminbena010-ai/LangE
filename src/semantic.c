#include "semantic.h"
#include "builtins.h"
#include "suggest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void semantic_warning(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message
);


/* =========================================================
 * UTILIDADES INTERNAS
 * ========================================================= */

static char *semantic_strdup(const char *text)
{
    if (text == NULL)
        return NULL;

    size_t length = strlen(text);

    char *copy = malloc(length + 1);

    if (copy == NULL)
        return NULL;

    memcpy(copy, text, length + 1);

    return copy;
}


static void semantic_set_error(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message
)
{
    if (context == NULL)
        return;

    semantic_error(
        context,
        kind,
        line,
        column,
        message
    );
}


static void semantic_set_error_suggest(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message,
    const char *suggestion
)
{
    if (context == NULL)
        return;

    semantic_error_hint(
        context,
        kind,
        line,
        column,
        message,
        suggestion
    );
}


static int semantic_is_void_type(const Type *type)
{
    return type == NULL;
}


static const char *semantic_type_name(const Type *type)
{
    if (type == NULL)
        return "void";

    switch (type->kind) {

        case TYPE_INT:
            return "int";

        case TYPE_FLOAT:
            return "float";

        case TYPE_STRING:
            return "string";

        case TYPE_BOOL:
            return "bool";

        case TYPE_CHAR:
            return "char";

        case TYPE_NULL:
            return "null";

        case TYPE_LIST:
            return "list";

        case TYPE_MAP:
            return "map";

        case TYPE_TASK:
            return "task";

        case TYPE_FUNCTION:
            return "function";

        case TYPE_STRUCT:
            return type->struct_name != NULL
                ? type->struct_name
                : "struct";

        case TYPE_I8:
            return "i8";

        case TYPE_I16:
            return "i16";

        case TYPE_I32:
            return "i32";

        case TYPE_U8:
            return "u8";

        case TYPE_U16:
            return "u16";

        case TYPE_U32:
            return "u32";

        case TYPE_U64:
            return "u64";

        case TYPE_SIZE:
            return "size";

        case TYPE_OPAQUE:
            return "opaque";

        case TYPE_CALLBACK:
            return "callback";

        case TYPE_BUFFER:
            return "buffer";

        default:
            return "tipo desconocido";
    }
}


static Type *semantic_copy_type(const Type *type)
{
    if (type == NULL)
        return NULL;

    Type *copy = malloc(sizeof(Type));

    if (copy == NULL)
        return NULL;

    copy->kind = type->kind;
    copy->element_type = NULL;
    copy->key_type = NULL;
    copy->value_type = NULL;
    copy->parameter_types = NULL;
    copy->parameter_count = 0;
    copy->return_type = NULL;
    copy->struct_name = NULL;

    if (type->struct_name != NULL) {
        copy->struct_name =
            semantic_strdup(type->struct_name);

        if (copy->struct_name == NULL) {
            free(copy);
            return NULL;
        }
    }

    if (type->element_type != NULL) {

        copy->element_type =
            semantic_copy_type(type->element_type);

        if (copy->element_type == NULL) {
            semantic_destroy_type(copy);
            return NULL;
        }
    }

    if (type->key_type != NULL) {

        copy->key_type =
            semantic_copy_type(type->key_type);

        if (copy->key_type == NULL) {
            semantic_destroy_type(copy);
            return NULL;
        }
    }

    if (type->value_type != NULL) {

        copy->value_type =
            semantic_copy_type(type->value_type);

        if (copy->value_type == NULL) {
            semantic_destroy_type(copy);
            return NULL;
        }
    }

    if (type->return_type != NULL) {
        copy->return_type =
            semantic_copy_type(type->return_type);

        if (copy->return_type == NULL) {
            semantic_destroy_type(copy);
            return NULL;
        }
    }

    if (type->parameter_count > 0) {
        copy->parameter_types =
            calloc((size_t) type->parameter_count,
                   sizeof(Type *));

        if (copy->parameter_types == NULL) {
            semantic_destroy_type(copy);
            return NULL;
        }

        for (int i = 0; i < type->parameter_count; i++) {
            copy->parameter_types[i] =
                semantic_copy_type(type->parameter_types[i]);

            if (copy->parameter_types[i] == NULL) {
                semantic_destroy_type(copy);
                return NULL;
            }
            copy->parameter_count++;
        }
    }

    return copy;
}


/* =========================================================
 * TIPOS
 * ========================================================= */

Type *semantic_create_basic_type(TypeKind kind)
{
    Type *type = malloc(sizeof(Type));

    if (type == NULL)
        return NULL;

    type->kind = kind;
    type->element_type = NULL;
    type->key_type = NULL;
    type->value_type = NULL;
    type->parameter_types = NULL;
    type->parameter_count = 0;
    type->return_type = NULL;
    type->struct_name = NULL;

    return type;
}


Type *semantic_create_list_type(Type *element_type)
{
    Type *type =
        semantic_create_basic_type(TYPE_LIST);

    if (type == NULL)
        return NULL;

    type->element_type = element_type;

    return type;
}


Type *semantic_create_map_type(
    Type *key_type,
    Type *value_type
)
{
    Type *type =
        semantic_create_basic_type(TYPE_MAP);

    if (type == NULL)
        return NULL;

    type->key_type = key_type;
    type->value_type = value_type;

    return type;
}


static Type *semantic_create_struct_type(
    const char *name
)
{
    if (name == NULL)
        return NULL;

    Type *type =
        semantic_create_basic_type(TYPE_STRUCT);

    if (type == NULL)
        return NULL;

    type->struct_name = semantic_strdup(name);

    if (type->struct_name == NULL) {
        semantic_destroy_type(type);
        return NULL;
    }

    return type;
}


void semantic_destroy_type(Type *type)
{
    if (type == NULL)
        return;

    semantic_destroy_type(type->element_type);
    semantic_destroy_type(type->key_type);
    semantic_destroy_type(type->value_type);
    semantic_destroy_type(type->return_type);
    free(type->struct_name);

    for (int i = 0; i < type->parameter_count; i++)
        semantic_destroy_type(type->parameter_types[i]);

    free(type->parameter_types);

    free(type);
}


int semantic_types_equal(
    const Type *a,
    const Type *b
)
{
    if (a == NULL || b == NULL)
        return a == b;

    if (a->kind != b->kind)
        return 0;

    switch (a->kind) {

        case TYPE_LIST:
            return semantic_types_equal(
                a->element_type,
                b->element_type
            );

        case TYPE_MAP:
            return
                semantic_types_equal(
                    a->key_type,
                    b->key_type
                ) &&
                semantic_types_equal(
                    a->value_type,
                    b->value_type
                );

        case TYPE_TASK:
            return semantic_types_equal(
                a->element_type,
                b->element_type
            );

        case TYPE_FUNCTION:
            if (a->parameter_count != b->parameter_count ||
                !semantic_types_equal(a->return_type, b->return_type))
                return 0;

            for (int i = 0; i < a->parameter_count; i++) {
                if (!semantic_types_equal(
                        a->parameter_types[i],
                        b->parameter_types[i]))
                    return 0;
            }

            return 1;

        case TYPE_STRUCT:
            return a->struct_name != NULL &&
                   b->struct_name != NULL &&
                   strcmp(a->struct_name, b->struct_name) == 0;

        default:
            return 1;
    }
}


/* Declaraciones adelantadas de helpers FFI (definidos
 * más abajo, junto a semantic_check_external_signature). */
static int semantic_type_is_ffi_integer(const Type *type);
static int semantic_type_is_ffi_only(const Type *type);
static int semantic_type_is_signed_integer(const Type *type);
static int semantic_ffi_integer_compatible(
    const Type *expected,
    const Type *actual
);
static int semantic_struct_is_pod(
    SemanticContext *context,
    const Type *type
);


int semantic_type_compatible(
    const Type *expected,
    const Type *actual
)
{
    if (expected == NULL)
        return actual == NULL;

    if (actual == NULL)
        return 0;

    if (semantic_types_equal(expected, actual))
        return 1;

    /*
     * Enteros FFI de ancho fijo ↔ int: la promoción /
     * estrechamiento es seguro en la frontera FFI.
     */
    if (semantic_ffi_integer_compatible(expected, actual))
        return 1;

    /*
     * callback<...> es compatible con un valor-función
     * (TYPE_FUNCTION) de firma equivalente, y viceversa.
     */
    if ((expected->kind == TYPE_CALLBACK &&
         actual->kind == TYPE_FUNCTION) ||
        (expected->kind == TYPE_FUNCTION &&
         actual->kind == TYPE_CALLBACK) ||
        (expected->kind == TYPE_CALLBACK &&
         actual->kind == TYPE_CALLBACK)) {

        if (expected->parameter_count !=
                actual->parameter_count)
            return 0;

        if (!semantic_type_compatible(
                expected->return_type,
                actual->return_type
            ))
            return 0;

        for (int i = 0;
             i < expected->parameter_count;
             i++) {

            if (!semantic_type_compatible(
                    expected->parameter_types[i],
                    actual->parameter_types[i]
                ))
                return 0;
        }

        return 1;
    }

    /*
     * buffer<T> es compatible con list<U> si U es
     * compatible con T. El código E++ pasa una list y
     * el compilador extrae puntero + longitud.
     */
    if (expected->kind == TYPE_BUFFER &&
        actual->kind == TYPE_LIST) {

        return semantic_type_compatible(
            expected->element_type,
            actual->element_type
        );
    }

    /*
     * null puede asignarse a tipos que admitan null.
     */
    if (actual->kind == TYPE_NULL)
        return semantic_is_nullable(expected);

    return 0;
}


int semantic_is_boolean(const Type *type)
{
    return type != NULL &&
           type->kind == TYPE_BOOL;
}


int semantic_is_numeric(const Type *type)
{
    if (type == NULL)
        return 0;

    return
        type->kind == TYPE_INT ||
        type->kind == TYPE_FLOAT;
}


int semantic_is_nullable(const Type *type)
{
    if (type == NULL)
        return 0;

    /*
     * Actualmente todos los tipos de referencia pueden
     * considerarse anulables.
     */
    return
        type->kind == TYPE_STRING ||
        type->kind == TYPE_LIST ||
        type->kind == TYPE_MAP ||
        type->kind == TYPE_NULL ||
        type->kind == TYPE_OPAQUE;
}


/* =========================================================
 * OPERADORES
 * ========================================================= */

Type *semantic_binary_result_type(
    ASTOperator operator,
    const Type *left,
    const Type *right
)
{
    if (left == NULL || right == NULL)
        return NULL;

    switch (operator) {

        case AST_OP_ADD:

            if (left->kind == TYPE_STRING &&
                right->kind == TYPE_STRING) {

                return semantic_create_basic_type(
                    TYPE_STRING
                );
            }

            if (semantic_is_numeric(left) &&
                semantic_is_numeric(right)) {

                if (left->kind == TYPE_FLOAT ||
                    right->kind == TYPE_FLOAT) {

                    return semantic_create_basic_type(
                        TYPE_FLOAT
                    );
                }

                return semantic_create_basic_type(
                    TYPE_INT
                );
            }

            if (left->kind == TYPE_LIST &&
                right->kind == TYPE_LIST &&
                semantic_types_equal(
                    left->element_type,
                    right->element_type
                )) {

                return semantic_copy_type(left);
            }

            return NULL;


        case AST_OP_SUB:
        case AST_OP_MUL:
        case AST_OP_DIV:
        case AST_OP_MOD:

            if (!semantic_is_numeric(left) ||
                !semantic_is_numeric(right)) {

                return NULL;
            }

            if (left->kind == TYPE_FLOAT ||
                right->kind == TYPE_FLOAT) {

                return semantic_create_basic_type(
                    TYPE_FLOAT
                );
            }

            return semantic_create_basic_type(
                TYPE_INT
            );

        case AST_OP_FLOOR_DIV:
            if (left->kind != TYPE_INT ||
                right->kind != TYPE_INT) {
                return NULL;
            }

            return semantic_create_basic_type(TYPE_INT);


        case AST_OP_EQUAL:
        case AST_OP_NOT_EQUAL:

            /*
             * listas y maps no se comparan (§13: == / != solo
             * entre escalares y strings). En IR los dos son
             * punteros opacos y el codegen acabaría leyendo
             * structs como si fueran cadenas (arreglo
             * 2026-10-07).
             */
            if (left->kind == TYPE_LIST ||
                left->kind == TYPE_MAP ||
                right->kind == TYPE_LIST ||
                right->kind == TYPE_MAP) {

                return NULL;
            }

            if (!semantic_type_compatible(
                    left,
                    right
                ) &&
                !semantic_type_compatible(
                    right,
                    left
                )) {

                return NULL;
            }

            return semantic_create_basic_type(
                TYPE_BOOL
            );


        case AST_OP_GREATER:
        case AST_OP_LESS:
        case AST_OP_GREATER_EQUAL:
        case AST_OP_LESS_EQUAL:

            if (semantic_is_numeric(left) &&
                semantic_is_numeric(right)) {

                return semantic_create_basic_type(
                    TYPE_BOOL
                );
            }

            if (left->kind == TYPE_STRING &&
                right->kind == TYPE_STRING) {

                return semantic_create_basic_type(
                    TYPE_BOOL
                );
            }

            return NULL;


        case AST_OP_AND:
        case AST_OP_OR:

            if (!semantic_is_boolean(left) ||
                !semantic_is_boolean(right)) {

                return NULL;
            }

            return semantic_create_basic_type(
                TYPE_BOOL
            );


        default:
            return NULL;
    }
}


Type *semantic_unary_result_type(
    ASTOperator operator,
    const Type *operand
)
{
    if (operand == NULL)
        return NULL;

    switch (operator) {

        case AST_OP_NEGATE:

            if (!semantic_is_numeric(operand))
                return NULL;

            return semantic_copy_type(operand);


        case AST_OP_NOT:

            if (!semantic_is_boolean(operand))
                return NULL;

            return semantic_create_basic_type(
                TYPE_BOOL
            );

        default:
            return NULL;
    }
}


/* =========================================================
 * SÍMBOLOS
 * ========================================================= */

static void semantic_symbol_destroy(
    SemanticSymbol *symbol
)
{
    if (symbol == NULL)
        return;

    free(symbol->name);

    semantic_destroy_type(symbol->type);

    if (symbol->function != NULL) {

        SemanticFunction *function =
            symbol->function;

        free(function->name);

        for (int i = 0;
             i < function->parameter_count;
             i++) {

            SemanticParameter *parameter =
                function->parameters[i];

            if (parameter == NULL)
                continue;

            free(parameter->name);
            semantic_destroy_type(parameter->type);
            free(parameter);
        }

        free(function->parameters);
        semantic_destroy_type(function->return_type);

        free(function);
    }

    free(symbol);
}


static SemanticSymbol *semantic_lookup_struct(
    SemanticContext *context,
    const char *name
)
{
    SemanticSymbol *symbol =
        semantic_lookup_symbol(context, name);

    return symbol != NULL &&
           symbol->kind == SYMBOL_STRUCT
        ? symbol
        : NULL;
}


static int semantic_validate_type(
    SemanticContext *context,
    const Type *type,
    int line,
    int column
)
{
    if (type == NULL)
        return 1;

    switch (type->kind) {

        case TYPE_STRUCT:
            if (semantic_lookup_struct(
                    context,
                    type->struct_name
                ) != NULL)
                return 1;

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                line,
                column,
                "tipo de struct no declarado"
            );
            return 0;

        case TYPE_LIST:
        case TYPE_TASK:
            return semantic_validate_type(
                context,
                type->element_type,
                line,
                column
            );

        case TYPE_MAP:
            return semantic_validate_type(
                       context,
                       type->key_type,
                       line,
                       column
                   ) &&
                   semantic_validate_type(
                       context,
                       type->value_type,
                       line,
                       column
                   );

        case TYPE_FUNCTION:
            if (!semantic_validate_type(
                    context,
                    type->return_type,
                    line,
                    column
                ))
                return 0;

            for (int i = 0;
                 i < type->parameter_count;
                 i++) {
                if (!semantic_validate_type(
                        context,
                        type->parameter_types[i],
                        line,
                        column
                    ))
                    return 0;
            }

            return 1;

        default:
            return 1;
    }
}


static int semantic_type_reaches_struct(
    SemanticContext *context,
    const Type *type,
    const char *target,
    const char **path,
    int path_count,
    int path_capacity
)
{
    if (type == NULL)
        return 0;

    if (type->kind == TYPE_LIST ||
        type->kind == TYPE_TASK)
        return semantic_type_reaches_struct(
            context,
            type->element_type,
            target,
            path,
            path_count,
            path_capacity
        );

    if (type->kind == TYPE_MAP)
        return semantic_type_reaches_struct(
                   context,
                   type->key_type,
                   target,
                   path,
                   path_count,
                   path_capacity
               ) ||
               semantic_type_reaches_struct(
                   context,
                   type->value_type,
                   target,
                   path,
                   path_count,
                   path_capacity
               );

    if (type->kind != TYPE_STRUCT ||
        type->struct_name == NULL)
        return 0;

    if (strcmp(type->struct_name, target) == 0)
        return 1;

    for (int i = 0; i < path_count; i++) {
        if (strcmp(path[i], type->struct_name) == 0)
            return 0;
    }

    if (path_count >= path_capacity)
        return 0;

    path[path_count] = type->struct_name;

    SemanticSymbol *symbol =
        semantic_lookup_struct(
            context,
            type->struct_name
        );

    if (symbol == NULL ||
        symbol->struct_decl == NULL)
        return 0;

    ASTNodeList *fields =
        &symbol->struct_decl->data.struct_decl.fields;

    for (int i = 0; i < fields->count; i++) {
        ASTNode *field = fields->items[i];

        if (field != NULL &&
            semantic_type_reaches_struct(
                context,
                field->data.var_decl.type,
                target,
                path,
                path_count + 1,
                path_capacity
            ))
            return 1;
    }

    return 0;
}


static int semantic_struct_type_is_printable(
    SemanticContext *context,
    const Type *type,
    const char **path,
    int path_count,
    int path_capacity
)
{
    if (type == NULL)
        return 0;

    switch (type->kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_CHAR:
            return 1;

        case TYPE_STRUCT:
            if (type->struct_name == NULL ||
                path_count >= path_capacity)
                return 0;

            for (int i = 0; i < path_count; i++) {
                if (strcmp(path[i], type->struct_name) == 0)
                    return 0;
            }

            path[path_count] = type->struct_name;

            {
                SemanticSymbol *symbol =
                    semantic_lookup_struct(
                        context,
                        type->struct_name
                    );

                if (symbol == NULL ||
                    symbol->struct_decl == NULL)
                    return 0;

                ASTNodeList *fields =
                    &symbol->struct_decl
                        ->data.struct_decl.fields;

                for (int i = 0; i < fields->count; i++) {
                    ASTNode *field = fields->items[i];

                    if (field == NULL ||
                        !semantic_struct_type_is_printable(
                            context,
                            field->data.var_decl.type,
                            path,
                            path_count + 1,
                            path_capacity
                        ))
                        return 0;
                }
            }

            return 1;

        default:
            return 0;
    }
}


static int semantic_type_is_printable(
    SemanticContext *context,
    const Type *type
)
{
    if (type == NULL)
        return 0;

    if (type->kind != TYPE_STRUCT)
        return type->kind == TYPE_INT ||
               type->kind == TYPE_FLOAT ||
               type->kind == TYPE_STRING ||
               type->kind == TYPE_BOOL ||
               type->kind == TYPE_CHAR;

    int capacity = 1;

    for (SemanticScope *scope = context->current_scope;
         scope != NULL;
         scope = scope->parent)
        capacity += scope->symbols.count;

    const char **path =
        calloc((size_t) capacity, sizeof(const char *));

    if (path == NULL)
        return 0;

    int printable =
        semantic_struct_type_is_printable(
            context,
            type,
            path,
            0,
            capacity
        );

    free(path);
    return printable;
}


static int semantic_register_struct(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL ||
        node->kind != AST_STRUCT_DECL)
        return 0;

    const char *name =
        node->data.struct_decl.name;

    if (name == NULL)
        return 0;

    if (semantic_lookup_current_scope(
            context,
            name
        ) != NULL) {
        semantic_set_error(
            context,
            SEMANTIC_ERROR_DUPLICATE_NAME,
            node->line,
            node->column,
            "el nombre ya esta declarado"
        );
        return 0;
    }

    SemanticSymbol *symbol =
        calloc(1, sizeof(SemanticSymbol));

    if (symbol == NULL)
        return 0;

    symbol->name = semantic_strdup(name);
    symbol->type = semantic_create_struct_type(name);
    symbol->kind = SYMBOL_STRUCT;
    symbol->is_const = 1;
    symbol->is_initialized = 1;
    symbol->struct_decl = node;
    symbol->line = node->line;
    symbol->column = node->column;

    if (symbol->name == NULL ||
        symbol->type == NULL ||
        !semantic_declare_symbol(context, symbol)) {
        semantic_symbol_destroy(symbol);
        return 0;
    }

    return 1;
}


static int semantic_analyze_struct(
    SemanticContext *context,
    ASTNode *node
)
{
    const char *name =
        node->data.struct_decl.name;

    SemanticSymbol *symbol =
        semantic_lookup_struct(context, name);

    if (symbol == NULL ||
        symbol->struct_decl != node) {
        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "struct no registrado"
        );
        return 0;
    }

    ASTNodeList *fields =
        &node->data.struct_decl.fields;

    if (fields->count == 0) {
        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "un struct debe declarar al menos un campo"
        );
        return 0;
    }

    int valid = 1;

    for (int i = 0; i < fields->count; i++) {
        ASTNode *field = fields->items[i];

        if (field == NULL ||
            field->kind != AST_VAR_DECL)
            continue;

        for (int j = 0; j < i; j++) {
            ASTNode *previous = fields->items[j];

            if (previous != NULL &&
                previous->data.var_decl.name != NULL &&
                field->data.var_decl.name != NULL &&
                strcmp(
                    previous->data.var_decl.name,
                    field->data.var_decl.name
                ) == 0) {
                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_DUPLICATE_NAME,
                    field->line,
                    field->column,
                    "campo duplicado en el struct"
                );
                valid = 0;
                break;
            }
        }

        if (!semantic_validate_type(
                context,
                field->data.var_decl.type,
                field->line,
                field->column
            ))
            valid = 0;

        if (semantic_type_is_ffi_only(
                field->data.var_decl.type
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                field->line,
                field->column,
                "los tipos i8, i16, i32, u8, u16, u32, u64 "
                "y size solo estan disponibles en firmas "
                "de funciones externas (external)"
            );

            valid = 0;
        }

        int capacity = 1;

        for (SemanticScope *scope = context->current_scope;
             scope != NULL;
             scope = scope->parent)
            capacity += scope->symbols.count;

        const char **path =
            calloc((size_t) capacity, sizeof(const char *));

        if (path == NULL) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_UNKNOWN,
                field->line,
                field->column,
                "no se pudo validar la recursividad de los structs"
            );
            valid = 0;
        } else {
            if (semantic_type_reaches_struct(
                    context,
                    field->data.var_decl.type,
                    name,
                    path,
                    0,
                    capacity
                )) {
                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_INVALID_TYPE,
                    field->line,
                    field->column,
                    "los structs no pueden contenerse de forma recursiva"
                );
                valid = 0;
            }

            free(path);
        }
    }

    return valid;
}


static void semantic_symbol_table_init(
    SemanticSymbolTable *table
)
{
    if (table == NULL)
        return;

    table->count = 0;
    table->capacity =
        SEMANTIC_INITIAL_SYMBOL_CAPACITY;

    table->symbols =
        calloc(
            table->capacity,
            sizeof(SemanticSymbol *)
        );
}


static void semantic_symbol_table_destroy(
    SemanticSymbolTable *table
)
{
    if (table == NULL)
        return;

    for (int i = 0;
         i < table->count;
         i++) {

        semantic_symbol_destroy(
            table->symbols[i]
        );
    }

    free(table->symbols);

    table->symbols = NULL;
    table->count = 0;
    table->capacity = 0;
}


static int semantic_symbol_table_add(
    SemanticSymbolTable *table,
    SemanticSymbol *symbol
)
{
    if (table == NULL || symbol == NULL)
        return 0;

    if (table->count >= table->capacity) {

        int new_capacity =
            table->capacity * 2;

        SemanticSymbol **new_symbols =
            realloc(
                table->symbols,
                sizeof(SemanticSymbol *) *
                new_capacity
            );

        if (new_symbols == NULL)
            return 0;

        table->symbols = new_symbols;
        table->capacity = new_capacity;
    }

    table->symbols[table->count++] =
        symbol;

    return 1;
}


SemanticSymbol *semantic_lookup_current_scope(
    SemanticContext *context,
    const char *name
)
{
    if (context == NULL ||
        context->current_scope == NULL ||
        name == NULL)
        return NULL;

    SemanticSymbolTable *table =
        &context->current_scope->symbols;

    for (int i = table->count - 1;
         i >= 0;
         i--) {

        SemanticSymbol *symbol =
            table->symbols[i];

        if (symbol != NULL &&
            strcmp(symbol->name, name) == 0) {

            return symbol;
        }
    }

    return NULL;
}


SemanticSymbol *semantic_lookup_symbol(
    SemanticContext *context,
    const char *name
)
{
    if (context == NULL || name == NULL)
        return NULL;

    SemanticScope *scope =
        context->current_scope;

    while (scope != NULL) {

        SemanticSymbolTable *table =
            &scope->symbols;

        for (int i = table->count - 1;
             i >= 0;
             i--) {

            SemanticSymbol *symbol =
                table->symbols[i];

            if (symbol != NULL &&
                strcmp(symbol->name, name) == 0) {

                return symbol;
            }
        }

        scope = scope->parent;
    }

    return NULL;
}


int semantic_symbol_exists(
    SemanticContext *context,
    const char *name
)
{
    return semantic_lookup_symbol(
        context,
        name
    ) != NULL;
}


int semantic_declare_symbol(
    SemanticContext *context,
    SemanticSymbol *symbol
)
{
    if (context == NULL ||
        context->current_scope == NULL ||
        symbol == NULL)
        return 0;

    if (semantic_lookup_current_scope(
            context,
            symbol->name
        ) != NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_DUPLICATE_NAME,
            symbol->line,
            symbol->column,
            "el nombre ya esta declarado"
        );

        return 0;
    }

    if (!semantic_symbol_table_add(
            &context->current_scope->symbols,
            symbol
        )) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            symbol->line,
            symbol->column,
            "no se pudo registrar el simbolo"
        );

        return 0;
    }

    return 1;
}


/* =========================================================
 * FUNCIONES SEMÁNTICAS
 * ========================================================= */

static void semantic_function_destroy(
    SemanticFunction *function
)
{
    if (function == NULL)
        return;

    free(function->name);

    for (int i = 0;
         i < function->parameter_count;
         i++) {

        SemanticParameter *parameter =
            function->parameters[i];

        if (parameter == NULL)
            continue;

        free(parameter->name);
        semantic_destroy_type(parameter->type);
        free(parameter);
    }

    free(function->parameters);

    semantic_destroy_type(
        function->return_type
    );

    free(function);
}


static SemanticFunction *semantic_function_from_ast(
    ASTNode *node
)
{
    if (node == NULL ||
        node->kind != AST_FUNCTION_DECL)
        return NULL;

    SemanticFunction *function =
        calloc(1, sizeof(SemanticFunction));

    if (function == NULL)
        return NULL;

    function->name =
        semantic_strdup(
            node->data.function_decl.name
        );

    function->return_type =
        semantic_copy_type(
            node->data.function_decl.return_type
        );

    function->is_private =
        node->data.function_decl.is_private;

    function->is_async =
        node->data.function_decl.is_async;

    function->external_string_owned =
        node->data.function_decl.external_string_owned;

    function->is_variadic =
        node->data.function_decl.is_variadic;

    function->parameter_count = 0;
    function->parameter_capacity =
        node->data.function_decl.parameters.count;

    if (function->parameter_capacity > 0) {

        function->parameters =
            calloc(
                function->parameter_capacity,
                sizeof(SemanticParameter *)
            );

        if (function->parameters == NULL) {

            semantic_function_destroy(function);
            return NULL;
        }
    }

    for (int i = 0;
         i < node->data.function_decl.parameters.count;
         i++) {

        ASTParameter *ast_parameter =
            node->data.function_decl.parameters.items[i];

        SemanticParameter *parameter =
            calloc(
                1,
                sizeof(SemanticParameter)
            );

        if (parameter == NULL) {

            semantic_function_destroy(function);
            return NULL;
        }

        parameter->name =
            semantic_strdup(
                ast_parameter->name
            );

        parameter->type =
            semantic_copy_type(
                ast_parameter->type
            );

        parameter->is_private =
            ast_parameter->is_private;

        function->parameters[
            function->parameter_count++
        ] = parameter;
    }

    return function;
}


static Type *semantic_type_from_function(
    const SemanticFunction *function
)
{
    if (function == NULL)
        return NULL;

    Type *type =
        semantic_create_basic_type(TYPE_FUNCTION);

    if (type == NULL)
        return NULL;

    type->return_type =
        semantic_copy_type(function->return_type);

    if (function->return_type != NULL &&
        type->return_type == NULL) {
        semantic_destroy_type(type);
        return NULL;
    }

    if (function->parameter_count > 0) {
        type->parameter_types =
            calloc((size_t) function->parameter_count,
                   sizeof(Type *));

        if (type->parameter_types == NULL) {
            semantic_destroy_type(type);
            return NULL;
        }

        for (int i = 0; i < function->parameter_count; i++) {
            type->parameter_types[i] =
                semantic_copy_type(function->parameters[i]->type);

            if (type->parameter_types[i] == NULL) {
                semantic_destroy_type(type);
                return NULL;
            }
            type->parameter_count++;
        }
    }

    return type;
}


/*
 * Reglas de funciones async (§52): main debe ser síncrono.
 */
static int semantic_check_async_function(
    SemanticContext *context,
    const SemanticFunction *function,
    int line,
    int column
)
{
    if (function == NULL || !function->is_async)
        return 1;

    if (function->name != NULL &&
        strcmp(function->name, "main") == 0) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            line,
            column,
            "main no puede ser async"
        );

        return 0;
    }

    return 1;
}


SemanticFunction *semantic_lookup_function(
    SemanticContext *context,
    const char *name
)
{
    SemanticSymbol *symbol =
        semantic_lookup_symbol(
            context,
            name
        );

    if (symbol == NULL ||
        symbol->kind != SYMBOL_FUNCTION)
        return NULL;

    return symbol->function;
}


/* =========================================================
 * SCOPES
 * ========================================================= */

int semantic_enter_scope(
    SemanticContext *context
)
{
    if (context == NULL)
        return 0;

    if (context->scope_depth >=
        SEMANTIC_MAX_SCOPE_DEPTH) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            0,
            0,
            "se alcanzo la profundidad maxima de scopes"
        );

        return 0;
    }

    SemanticScope *scope =
        calloc(1, sizeof(SemanticScope));

    if (scope == NULL)
        return 0;

    semantic_symbol_table_init(
        &scope->symbols
    );

    scope->parent =
        context->current_scope;

    scope->depth =
        context->scope_depth + 1;

    context->current_scope = scope;
    context->scope_depth++;

    return 1;
}


int semantic_leave_scope(
    SemanticContext *context
)
{
    if (context == NULL ||
        context->current_scope == NULL)
        return 0;

    SemanticScope *scope =
        context->current_scope;

    context->current_scope =
        scope->parent;

    /*
     * Aviso de variable local no usada. Se excluyen los
     * parametros y los nombres que empiezan por "_".
     */
    if (scope->depth >= 2) {

        for (int i = 0;
             i < scope->symbols.count;
             i++) {

            SemanticSymbol *symbol =
                scope->symbols.symbols[i];

            if (symbol == NULL ||
                symbol->kind != SYMBOL_VARIABLE ||
                symbol->is_used ||
                symbol->name == NULL ||
                symbol->name[0] == '_' ||
                symbol->line <= 0)
            {
                continue;
            }

            char message[512];

            snprintf(
                message,
                sizeof(message),
                "la variable '%s' no se usa",
                symbol->name
            );

            semantic_warning(
                context,
                SEMANTIC_WARNING_UNUSED_VARIABLE,
                symbol->line,
                symbol->column,
                message
            );
        }
    }

    semantic_symbol_table_destroy(
        &scope->symbols
    );

    free(scope);

    if (context->scope_depth > 0)
        context->scope_depth--;

    return 1;
}


SemanticScope *semantic_current_scope(
    SemanticContext *context
)
{
    if (context == NULL)
        return NULL;

    return context->current_scope;
}


/* =========================================================
 * CONTEXTO
 * ========================================================= */

SemanticContext *semantic_create(void)
{
    SemanticContext *context =
        calloc(
            1,
            sizeof(SemanticContext)
        );

    if (context == NULL)
        return NULL;

    context->global_scope =
        calloc(
            1,
            sizeof(SemanticScope)
        );

    if (context->global_scope == NULL) {

        free(context);
        return NULL;
    }

    semantic_symbol_table_init(
        &context->global_scope->symbols
    );

    context->global_scope->parent = NULL;
    context->global_scope->depth = 0;

    context->current_scope =
        context->global_scope;

    context->scope_depth = 0;
    context->loop_depth = 0;
    context->function_depth = 0;
    context->try_depth = 0;

    context->errors.errors =
        calloc(
            SEMANTIC_INITIAL_ERROR_CAPACITY,
            sizeof(SemanticError *)
        );

    context->errors.capacity =
        SEMANTIC_INITIAL_ERROR_CAPACITY;

    context->errors.count = 0;

    context->error_count = 0;
    context->had_error = 0;

    return context;
}


void semantic_destroy(
    SemanticContext *context
)
{
    if (context == NULL)
        return;

    while (
        context->current_scope != NULL &&
        context->current_scope != context->global_scope
    ) {
        semantic_leave_scope(context);
    }

    if (context->global_scope != NULL) {

        semantic_symbol_table_destroy(
            &context->global_scope->symbols
        );

        free(context->global_scope);
    }

    for (int i = 0;
         i < context->module_count;
         i++) {

        SemanticModule *module =
            context->modules[i];

        if (module == NULL)
            continue;

        if (module->scope != NULL) {

            semantic_symbol_table_destroy(
                &module->scope->symbols
            );

            free(module->scope);
        }

        for (int e = 0;
             e < module->exported_count;
             e++)
            free(module->exported[e]);

        free(module->exported);

        free(module->name);
        free(module);
    }

    free(context->modules);

    for (int i = 0;
         i < context->alias_count;
         i++)
        free(context->aliases[i].name);

    free(context->aliases);

    for (int i = 0;
         i < context->errors.count;
         i++) {

        SemanticError *error =
            context->errors.errors[i];

        if (error == NULL)
            continue;

        free(error->message);
        free(error->suggestion);
        free(error);
    }

    free(context->errors.errors);

    free(context);
}


/* =========================================================
 * ERRORES
 * ========================================================= */

void semantic_error_hint(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message,
    const char *suggestion
)
{
    if (context == NULL)
        return;

    context->had_error = 1;
    context->error_count++;

    if (context->errors.count >=
        context->errors.capacity) {

        int new_capacity =
            context->errors.capacity * 2;

        SemanticError **new_errors =
            realloc(
                context->errors.errors,
                sizeof(SemanticError *) *
                new_capacity
            );

        if (new_errors == NULL)
            return;

        context->errors.errors =
            new_errors;

        context->errors.capacity =
            new_capacity;
    }

    SemanticError *error =
        calloc(1, sizeof(SemanticError));

    if (error == NULL)
        return;

    error->kind = kind;
    error->line = line;
    error->column = column;

    error->message =
        semantic_strdup(message);

    if (suggestion != NULL) {
        error->suggestion =
            semantic_strdup(suggestion);
    }

    context->errors.errors[
        context->errors.count++
    ] = error;
}


/* =========================================================
 * AVISOS
 * ========================================================= */

void semantic_warning(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message
)
{
    if (context == NULL)
        return;

    if (context->errors.count >=
        context->errors.capacity) {

        int new_capacity =
            context->errors.capacity * 2;

        SemanticError **new_errors =
            realloc(
                context->errors.errors,
                sizeof(SemanticError *) *
                new_capacity
            );

        if (new_errors == NULL)
            return;

        context->errors.errors = new_errors;
        context->errors.capacity = new_capacity;
    }

    SemanticError *error =
        calloc(1, sizeof(SemanticError));

    if (error == NULL)
        return;

    error->kind = kind;
    error->line = line;
    error->column = column;
    error->message = semantic_strdup(message);
    error->is_warning = 1;

    context->errors.errors[
        context->errors.count++
    ] = error;
}


/* =========================================================
 * SUGERENCIAS DE NOMBRES (REM)
 * ========================================================= */

static const char *const semantic_builtin_names[] = {
    "print", "input", "len"
};

static const char *const semantic_stdlib_modules[] = {
    "color", "csv", "date", "files", "json", "math",
    "paths", "random", "statistics", "system", "text",
    "time", "validation"
};


/*
 * Devuelve el nombre visible mas parecido a word
 * o NULL si ninguno queda dentro del umbral.
 */
static const char *semantic_suggest_name(
    SemanticContext *context,
    const char *word,
    int functions_only
)
{
    if (context == NULL ||
        word == NULL ||
        word[0] == '\0')
        return NULL;

    int builtin_count =
        (int)(sizeof(semantic_builtin_names) /
              sizeof(semantic_builtin_names[0]));

    int module_count = functions_only
        ? 0
        : (int)(sizeof(semantic_stdlib_modules) /
                sizeof(semantic_stdlib_modules[0]));

    int visible = 0;

    for (SemanticScope *scope = context->current_scope;
         scope != NULL;
         scope = scope->parent) {

        for (int i = 0;
             i < scope->symbols.count;
             i++) {

            SemanticSymbol *symbol =
                scope->symbols.symbols[i];

            if (symbol == NULL ||
                symbol->name == NULL)
                continue;

            if (functions_only &&
                symbol->kind != SYMBOL_FUNCTION)
                continue;

            visible++;
        }
    }

    int capacity =
        visible + builtin_count + module_count;

    if (capacity <= 0)
        return NULL;

    const char **candidates =
        malloc(sizeof(const char *) * capacity);

    if (candidates == NULL)
        return NULL;

    int count = 0;

    for (SemanticScope *scope = context->current_scope;
         scope != NULL;
         scope = scope->parent) {

        for (int i = 0;
             i < scope->symbols.count;
             i++) {

            SemanticSymbol *symbol =
                scope->symbols.symbols[i];

            if (symbol == NULL ||
                symbol->name == NULL)
                continue;

            if (functions_only &&
                symbol->kind != SYMBOL_FUNCTION)
                continue;

            candidates[count++] = symbol->name;
        }
    }

    for (int i = 0; i < builtin_count; i++) {
        candidates[count++] =
            semantic_builtin_names[i];
    }

    for (int i = 0; i < module_count; i++) {
        candidates[count++] =
            semantic_stdlib_modules[i];
    }

    const char *best =
        suggest_best(word, candidates, count);

    free(candidates);

    return best;
}


/*
 * Devuelve el nombre exportado por el modulo mas
 * parecido a word o NULL.
 */
static const char *semantic_suggest_module_fn(
    SemanticModule *module,
    const char *word
)
{
    if (module == NULL ||
        word == NULL ||
        word[0] == '\0' ||
        module->exported == NULL ||
        module->exported_count <= 0)
        return NULL;

    const char **candidates =
        malloc(
            sizeof(const char *) *
            module->exported_count
        );

    if (candidates == NULL)
        return NULL;

    int count = 0;

    for (int i = 0;
         i < module->exported_count;
         i++) {

        if (module->exported[i] == NULL)
            continue;

        candidates[count++] =
            module->exported[i];
    }

    const char *best = NULL;

    if (count > 0) {
        best = suggest_best(
            word,
            candidates,
            count
        );
    }

    free(candidates);

    return best;
}


/*
 * Registra un error; si best no es NULL anade
 * "quizas quisiste decir 'best'".
 */
static void semantic_set_error_suggesting(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message,
    const char *best
)
{
    if (best == NULL) {
        semantic_set_error(
            context,
            kind,
            line,
            column,
            message
        );

        return;
    }

    char suggestion[128];

    snprintf(
        suggestion,
        sizeof(suggestion),
        "quizas quisiste decir '%s'",
        best
    );

    semantic_set_error_suggest(
        context,
        kind,
        line,
        column,
        message,
        suggestion
    );
}


void semantic_error(
    SemanticContext *context,
    SemanticErrorKind kind,
    int line,
    int column,
    const char *message
)
{
    semantic_error_hint(
        context,
        kind,
        line,
        column,
        message,
        NULL
    );
}


int semantic_error_count(
    const SemanticContext *context
)
{
    if (context == NULL)
        return 0;

    return context->errors.count;
}


int semantic_has_error(
    const SemanticContext *context
)
{
    if (context == NULL)
        return 1;

    return context->had_error;
}


const SemanticError *semantic_get_error(
    const SemanticContext *context,
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


void semantic_clear_errors(
    SemanticContext *context
)
{
    if (context == NULL)
        return;

    for (int i = 0;
         i < context->errors.count;
         i++) {

        SemanticError *error =
            context->errors.errors[i];

        if (error == NULL)
            continue;

        free(error->message);
        free(error->suggestion);
        free(error);
    }

    context->errors.count = 0;
    context->error_count = 0;
    context->had_error = 0;
}


/*
 * Clase estilo Python del error semántico, usada en los
 * diagnosticos impresos (SyntaxError, TypeError, ...).
 */
const char *semantic_error_class_name(
    SemanticErrorKind kind
)
{
    switch (kind) {

        /* Nombres */
        case SEMANTIC_ERROR_UNDEFINED_NAME:
        case SEMANTIC_ERROR_DUPLICATE_NAME:
        case SEMANTIC_ERROR_VARIABLE_NOT_INITIALIZED:
        case SEMANTIC_ERROR_FUNCTION_NOT_FOUND:
            return "NameError";

        /* Tipos */
        case SEMANTIC_ERROR_TYPE_MISMATCH:
        case SEMANTIC_ERROR_INVALID_TYPE:
        case SEMANTIC_ERROR_INVALID_OPERATION:
        case SEMANTIC_ERROR_ARGUMENT_COUNT:
        case SEMANTIC_ERROR_ARGUMENT_TYPE:
        case SEMANTIC_ERROR_RETURN_TYPE:
        case SEMANTIC_ERROR_INVALID_INDEX:
        case SEMANTIC_ERROR_INVALID_ELEMENT_TYPE:
        case SEMANTIC_ERROR_INVALID_KEY_TYPE:
        case SEMANTIC_ERROR_NULL_NOT_ALLOWED:
        case SEMANTIC_ERROR_INVALID_ASSIGNMENT:
            return "TypeError";

        /* Constantes */
        case SEMANTIC_ERROR_ASSIGN_TO_CONST:
            return "ValueError";

        /* Saltos invalidos (igual que en Python) */
        case SEMANTIC_ERROR_INVALID_BREAK:
        case SEMANTIC_ERROR_INVALID_CONTINUE:
        case SEMANTIC_ERROR_RETURN_OUTSIDE_FUNCTION:
            return "SyntaxError";

        /* Visibilidad */
        case SEMANTIC_ERROR_PRIVATE_ACCESS:
            return "AttributeError";

        /* Imports */
        case SEMANTIC_ERROR_MODULE_NOT_FOUND:
        case SEMANTIC_ERROR_IMPORT_NOT_FOUND:
            return "ImportError";

        /* Avisos */
        case SEMANTIC_WARNING_UNUSED_VARIABLE:
            return "Warning";

        default:
            return "SystemError";
    }
}


/* =========================================================
 * VISIBILIDAD
 * ========================================================= */

int semantic_is_private_name(
    const char *name
)
{
    return name != NULL &&
           name[0] == '_';
}


int semantic_check_visibility(
    SemanticContext *context,
    SemanticSymbol *symbol
)
{
    if (context == NULL || symbol == NULL)
        return 0;

    if (!symbol->is_private)
        return 1;

    /*
     * Regla de alcance (decisión 2026-10-08, ver
     * claude.md §5):
     *
     * - Local o parámetro `_x`: solo se ve en el scope
     *   donde se declaró (lo garantizan los scopes; una
     *   función distinta da "name not defined").
     * - Función o global `_f` / `_g`: visible en todo el
     *   archivo (principal o módulo) donde está.
     * - Nada privado sale de su módulo: `from X import _f`
     *   y `X._f()` ya son error en la fase de imports
     *   (semantic_analyze_import / member call).
     *
     * Por tanto, dentro de la misma unidad de compilación
     * el acceso es válido.
     */
    return 1;
}


/* =========================================================
 * VARIABLES
 * ========================================================= */

/*
 * Literal vacío `[]` analizado con el tipo esperado.
 *
 * `var l:list<int> = []` y `l = []` no traen información de
 * elemento dentro del literal: el tipo se toma de la
 * anotación de la variable (decisión de diseño
 * 2026-10-06).
 *
 * Devuelve una copia de `expected` y pone `*handled` a 1 si
 * `value` es una lista o un map vacíos que encajan con el
 * tipo esperado. En cualquier otro caso `*handled` queda en
 * 0 y se devuelve NULL sin emitir error: el llamante sigue
 * con el análisis normal.
 */
static Type *semantic_analyze_empty_literal(
    SemanticContext *context,
    ASTNode *value,
    const Type *expected,
    int *handled
)
{
    if (handled == NULL)
        return NULL;

    *handled = 0;

    if (context == NULL ||
        value == NULL ||
        expected == NULL)
        return NULL;

    /*
     * El parser convierte cualquier `[]` vacío en un
     * literal de lista (maps no usan `{}`): decide el tipo
     * esperado de la anotación.
     */
    int is_empty_literal =
        (value->kind == AST_LIST_LITERAL &&
         value->data.list_literal.elements.count == 0) ||
        (value->kind == AST_MAP_LITERAL &&
         value->data.map_literal.entries.count == 0);

    if (!is_empty_literal)
        return NULL;

    if (expected->kind != TYPE_LIST &&
        expected->kind != TYPE_MAP)
        return NULL;

    Type *type =
        semantic_copy_type(expected);

    if (type == NULL)
        return NULL;

    semantic_destroy_type(value->expr_type);

    value->expr_type =
        semantic_copy_type(type);

    *handled = 1;

    return type;
}


int semantic_analyze_var(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    int infer_function_type =
        node->data.var_decl.type == NULL;

    Type *declared_type =
        semantic_copy_type(node->data.var_decl.type);

    if (declared_type != NULL &&
        !semantic_validate_type(
            context,
            declared_type,
            node->line,
            node->column
        )) {
        semantic_destroy_type(declared_type);
        return 0;
    }

    if (declared_type != NULL &&
        semantic_type_is_ffi_only(declared_type)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "los tipos i8, i16, i32, u8, u16, u32, u64 y "
            "size solo estan disponibles en firmas de "
            "funciones externas (external)"
        );

        semantic_destroy_type(declared_type);
        return 0;
    }

    if (!infer_function_type &&
        declared_type == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "tipo de variable invalido"
        );

        return 0;
    }

    int handled = 0;

    Type *value_type =
        semantic_analyze_empty_literal(
            context,
            node->data.var_decl.value,
            declared_type,
            &handled
        );

    if (!handled) {

        value_type =
            semantic_analyze_expression(
                context,
                node->data.var_decl.value
            );
    }

    if (value_type == NULL) {

        semantic_destroy_type(
            declared_type
        );

        return 0;
    }

    if (infer_function_type) {
        if (value_type->kind != TYPE_FUNCTION) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                node->line,
                node->column,
                "la inferencia de tipo solo esta disponible "
                "para variables de funcion"
            );
            semantic_destroy_type(value_type);
            return 0;
        }

        declared_type = semantic_copy_type(value_type);
        if (declared_type == NULL) {
            semantic_destroy_type(value_type);
            return 0;
        }
    }

    if (!semantic_type_compatible(
            declared_type,
            value_type
        )) {

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "tipo incompatible: se esperaba %s pero se encontro %s",
            semantic_type_name(declared_type),
            semantic_type_name(value_type)
        );

        semantic_set_error(
            context,
            SEMANTIC_ERROR_TYPE_MISMATCH,
            node->line,
            node->column,
            message
        );

        semantic_destroy_type(
            declared_type
        );

        semantic_destroy_type(
            value_type
        );

        return 0;
    }

    if (infer_function_type) {
        node->data.var_decl.type =
            semantic_copy_type(declared_type);

        if (node->data.var_decl.type == NULL) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                node->line,
                node->column,
                "no se pudo inferir el tipo de la funcion"
            );
            semantic_destroy_type(declared_type);
            semantic_destroy_type(value_type);
            return 0;
        }
    }

    SemanticSymbol *symbol =
        calloc(
            1,
            sizeof(SemanticSymbol)
        );

    if (symbol == NULL) {

        semantic_destroy_type(declared_type);
        semantic_destroy_type(value_type);

        return 0;
    }

    symbol->name =
        semantic_strdup(
            node->data.var_decl.name
        );

    symbol->type =
        declared_type;

    symbol->kind =
        SYMBOL_VARIABLE;

    symbol->is_private =
        node->data.var_decl.is_private;

    symbol->is_const = 0;
    symbol->is_initialized = 1;

    symbol->line = node->line;
    symbol->column = node->column;

    int result =
        semantic_declare_symbol(
            context,
            symbol
        );

    semantic_destroy_type(value_type);

    if (!result)
        semantic_symbol_destroy(symbol);

    return result;
}


/* =========================================================
 * CONSTANTES
 * ========================================================= */

int semantic_analyze_const(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    Type *declared_type =
        semantic_copy_type(
            node->data.const_decl.type
        );

    if (declared_type != NULL &&
        !semantic_validate_type(
            context,
            declared_type,
            node->line,
            node->column
        )) {
        semantic_destroy_type(declared_type);
        return 0;
    }

    if (declared_type != NULL &&
        semantic_type_is_ffi_only(declared_type)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "los tipos i8, i16, i32, u8, u16, u32, u64 y "
            "size solo estan disponibles en firmas de "
            "funciones externas (external)"
        );

        semantic_destroy_type(declared_type);
        return 0;
    }

    if (declared_type == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "tipo de constante invalido"
        );

        return 0;
    }

    int handled = 0;

    Type *value_type =
        semantic_analyze_empty_literal(
            context,
            node->data.const_decl.value,
            declared_type,
            &handled
        );

    if (!handled) {

        value_type =
            semantic_analyze_expression(
                context,
                node->data.const_decl.value
            );
    }

    if (value_type == NULL) {

        semantic_destroy_type(
            declared_type
        );

        return 0;
    }

    if (!semantic_type_compatible(
            declared_type,
            value_type
        )) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_TYPE_MISMATCH,
            node->line,
            node->column,
            "el valor no coincide con el tipo de la constante"
        );

        semantic_destroy_type(declared_type);
        semantic_destroy_type(value_type);

        return 0;
    }

    SemanticSymbol *symbol =
        calloc(
            1,
            sizeof(SemanticSymbol)
        );

    if (symbol == NULL) {

        semantic_destroy_type(declared_type);
        semantic_destroy_type(value_type);

        return 0;
    }

    symbol->name =
        semantic_strdup(
            node->data.const_decl.name
        );

    symbol->type = declared_type;
    symbol->kind = SYMBOL_CONSTANT;

    symbol->is_private =
        node->data.const_decl.is_private;

    symbol->is_const = 1;
    symbol->is_initialized = 1;

    symbol->line = node->line;
    symbol->column = node->column;

    int result =
        semantic_declare_symbol(
            context,
            symbol
        );

    semantic_destroy_type(value_type);

    if (!result)
        semantic_symbol_destroy(symbol);

    return result;
}


/* =========================================================
 * FUNCIONES
 * ========================================================= */

int semantic_analyze_function(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    SemanticFunction *function =
        semantic_function_from_ast(node);

    if (function == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            node->line,
            node->column,
            "no se pudo crear la funcion semantica"
        );

        return 0;
    }

    if (!semantic_check_async_function(
            context,
            function,
            node->line,
            node->column
        )) {

        semantic_function_destroy(function);
        return 0;
    }

    SemanticSymbol *symbol =
        calloc(
            1,
            sizeof(SemanticSymbol)
        );

    if (symbol == NULL) {

        semantic_function_destroy(function);
        return 0;
    }

    symbol->name =
        semantic_strdup(function->name);

    symbol->type =
        semantic_type_from_function(function);

    symbol->kind =
        SYMBOL_FUNCTION;

    symbol->is_private =
        function->is_private;

    symbol->is_const = 1;
    symbol->is_initialized = 1;
    symbol->function = function;

    symbol->line = node->line;
    symbol->column = node->column;

    if (!semantic_declare_symbol(
            context,
            symbol
        )) {

        semantic_symbol_destroy(symbol);
        return 0;
    }

    /*
     * Analizar cuerpo.
     *
     * Una función declarada dentro de un try no hereda la
     * restricción de return/break/continue: su cuerpo se
     * analiza con try_depth a cero.
     */
    SemanticFunction *previous_function =
        context->current_function;

    int previous_try_depth =
        context->try_depth;

    context->try_depth = 0;

    context->current_function =
        function;

    context->function_depth++;

    if (!semantic_enter_scope(context)) {

        context->current_function =
            previous_function;

        context->function_depth--;
        context->try_depth =
            previous_try_depth;

        return 0;
    }

    /*
     * Declarar parámetros.
     */
    for (int i = 0;
         i < function->parameter_count;
         i++) {

        SemanticParameter *parameter =
            function->parameters[i];

        SemanticSymbol *parameter_symbol =
            calloc(
                1,
                sizeof(SemanticSymbol)
            );

        if (parameter_symbol == NULL)
            continue;

        parameter_symbol->name =
            semantic_strdup(
                parameter->name
            );

        parameter_symbol->type =
            semantic_copy_type(
                parameter->type
            );

        parameter_symbol->kind =
            SYMBOL_PARAMETER;

        parameter_symbol->is_private =
            parameter->is_private;

        parameter_symbol->is_initialized = 1;

        parameter_symbol->line =
            node->line;

        parameter_symbol->column =
            node->column;

        if (!semantic_declare_symbol(
                context,
                parameter_symbol
            )) {

            semantic_symbol_destroy(
                parameter_symbol
            );
        }
    }

    semantic_analyze_block(
        context,
        node->data.function_decl.body
    );

    semantic_leave_scope(context);

    context->function_depth--;

    context->try_depth =
        previous_try_depth;

    context->current_function =
        previous_function;

    return 1;
}


/* =========================================================
 * IMPORTS
 *
 * Los imports solo se permiten a nivel global.
 *
 * El loader (main.c) resuelve los archivos .ro antes del
 * análisis semántico y adjunta las declaraciones de cada
 * módulo al nodo import correspondiente.
 * ========================================================= */

int semantic_analyze_import(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    semantic_set_error(
        context,
        SEMANTIC_ERROR_INVALID_OPERATION,
        node->line,
        node->column,
        "imports are only allowed at global level"
    );

    return 0;
}


int semantic_analyze_from_import(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    semantic_set_error(
        context,
        SEMANTIC_ERROR_INVALID_OPERATION,
        node->line,
        node->column,
        "imports are only allowed at global level"
    );

    return 0;
}


/* =========================================================
 * MÓDULOS
 * ========================================================= */

static int semantic_register_import(
    SemanticContext *context,
    ASTNode *node
);

static int semantic_register_from_import(
    SemanticContext *context,
    ASTNode *node
);


static SemanticModule *semantic_find_module(
    SemanticContext *context,
    const char *name
)
{
    if (context == NULL ||
        name == NULL)
        return NULL;

    for (int i = 0;
         i < context->module_count;
         i++) {

        SemanticModule *module =
            context->modules[i];

        if (module != NULL &&
            module->name != NULL &&
            strcmp(module->name, name) == 0)
            return module;
    }

    return NULL;
}


static SemanticScope *semantic_create_module_scope(void)
{
    SemanticScope *scope =
        calloc(1, sizeof(SemanticScope));

    if (scope == NULL)
        return NULL;

    semantic_symbol_table_init(
        &scope->symbols
    );

    /*
     * Sin parent: un módulo no ve los símbolos del
     * archivo principal.
     */
    scope->parent = NULL;
    scope->depth = 0;

    return scope;
}


static ASTNode *semantic_module_find_function(
    SemanticModule *module,
    const char *name
)
{
    if (module == NULL ||
        module->declarations == NULL ||
        name == NULL)
        return NULL;

    for (int i = 0;
         i < module->declarations->count;
         i++) {

        ASTNode *node =
            module->declarations->items[i];

        if (node == NULL ||
            node->kind != AST_FUNCTION_DECL)
            continue;

        if (node->data.function_decl.name != NULL &&
            strcmp(
                node->data.function_decl.name,
                name) == 0)
            return node;
    }

    return NULL;
}


static int semantic_module_add_exported(
    SemanticModule *module,
    const char *name
)
{
    if (module == NULL ||
        name == NULL)
        return 0;

    char **grown =
        realloc(
            module->exported,
            (size_t)(module->exported_count + 1) *
            sizeof(char *)
        );

    if (grown == NULL)
        return 0;

    module->exported = grown;

    module->exported[
        module->exported_count
    ] = semantic_strdup(name);

    if (module->exported[
            module->exported_count
        ] == NULL)
        return 0;

    module->exported_count++;

    return 1;
}


static int semantic_module_exported(
    SemanticModule *module,
    const char *name
)
{
    if (module == NULL ||
        name == NULL)
        return 0;

    for (int i = 0;
         i < module->exported_count;
         i++) {

        if (module->exported[i] != NULL &&
            strcmp(module->exported[i], name) == 0)
            return 1;
    }

    return 0;
}


static int semantic_declare_function_symbol(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL ||
        node->kind != AST_FUNCTION_DECL)
        return 0;

    SemanticFunction *function =
        semantic_function_from_ast(node);

    if (function == NULL)
        return 0;

    SemanticSymbol *symbol =
        calloc(
            1,
            sizeof(SemanticSymbol)
        );

    if (symbol == NULL) {

        semantic_function_destroy(function);
        return 0;
    }

    symbol->name =
        semantic_strdup(function->name);

    symbol->type =
        semantic_copy_type(
            function->return_type
        );

    symbol->kind = SYMBOL_FUNCTION;

    symbol->is_private =
        function->is_private;

    symbol->is_const = 1;
    symbol->is_initialized = 1;
    symbol->function = function;

    symbol->line = node->line;
    symbol->column = node->column;

    if (!semantic_declare_symbol(
            context,
            symbol
        )) {

        semantic_symbol_destroy(symbol);
        return 0;
    }

    return 1;
}


static SemanticModule *semantic_register_module(
    SemanticContext *context,
    const char *name,
    ASTNodeList *declarations,
    int line,
    int column
)
{
    if (context == NULL ||
        name == NULL)
        return NULL;

    SemanticModule *existing =
        semantic_find_module(context, name);

    if (existing != NULL)
        return existing;


    SemanticModule *module =
        calloc(1, sizeof(SemanticModule));

    if (module == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            line,
            column,
            "no se pudo reservar memoria para el modulo"
        );

        return NULL;
    }

    module->name =
        semantic_strdup(name);

    module->declarations = declarations;

    module->scope =
        semantic_create_module_scope();

    if (module->name == NULL ||
        module->scope == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            line,
            column,
            "no se pudo reservar memoria para el modulo"
        );

        free(module->name);
        free(module->scope);
        free(module);

        return NULL;
    }

    SemanticModule **grown =
        realloc(
            context->modules,
            (size_t)(context->module_count + 1) *
            sizeof(SemanticModule *)
        );

    if (grown == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            line,
            column,
            "no se pudo reservar memoria para el modulo"
        );

        free(module->name);
        semantic_symbol_table_destroy(
            &module->scope->symbols
        );
        free(module->scope);
        free(module);

        return NULL;
    }

    context->modules = grown;
    context->modules[
        context->module_count++
    ] = module;


    /*
     * Registrar las declaraciones dentro del scope
     * del módulo.
     */

    SemanticScope *previous =
        context->current_scope;

    context->current_scope = module->scope;

    if (declarations != NULL) {

        for (int i = 0;
             i < declarations->count;
             i++) {
            ASTNode *node = declarations->items[i];

            if (node != NULL &&
                node->kind == AST_STRUCT_DECL)
                semantic_register_struct(context, node);
        }

        for (int i = 0;
             i < declarations->count;
             i++) {

            ASTNode *node =
                declarations->items[i];

            if (node == NULL)
                continue;

            if (node->kind ==
                AST_FUNCTION_DECL)
            {
                const char *function_name =
                    node
                        ->data.function_decl.name;

                if (function_name != NULL &&
                    strcmp(function_name,
                           "main") == 0)
                {
                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_INVALID_OPERATION,
                        node->line,
                        node->column,
                        "no se permite 'main' en un modulo importado"
                    );
                }
                else if (
                    semantic_lookup_current_scope(
                        context,
                        function_name
                    ) != NULL)
                {
                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_DUPLICATE_NAME,
                        node->line,
                        node->column,
                        "la funcion ya esta declarada"
                    );
                }
                else {
                    semantic_declare_function_symbol(
                        context,
                        node
                    );
                }
            }
            else if (node->kind ==
                     AST_IMPORT)
            {
                semantic_register_import(
                    context,
                    node
                );
            }
            else if (node->kind ==
                     AST_FROM_IMPORT)
            {
                semantic_register_from_import(
                    context,
                    node
                );
            }
        }
    }

    context->current_scope = previous;

    return module;
}


static int semantic_register_alias(
    SemanticContext *context,
    const char *alias,
    SemanticModule *module,
    int line,
    int column
)
{
    if (context == NULL ||
        alias == NULL ||
        module == NULL)
        return 0;

    SemanticScope *owner =
        context->current_scope;

    for (int i = 0;
         i < context->alias_count;
         i++) {

        SemanticAlias *existing =
            &context->aliases[i];

        if (existing->owner == owner &&
            existing->name != NULL &&
            strcmp(existing->name, alias) == 0) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_DUPLICATE_NAME,
                line,
                column,
                "el alias ya esta declarado"
            );

            return 0;
        }
    }

    SemanticAlias *grown =
        realloc(
            context->aliases,
            (size_t)(context->alias_count + 1) *
            sizeof(SemanticAlias)
        );

    if (grown == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            line,
            column,
            "no se pudo reservar memoria para el alias"
        );

        return 0;
    }

    context->aliases = grown;

    SemanticAlias *entry =
        &context->aliases[
            context->alias_count
        ];

    entry->name =
        semantic_strdup(alias);

    entry->module = module;
    entry->owner = owner;

    if (entry->name == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_UNKNOWN,
            line,
            column,
            "no se pudo reservar memoria para el alias"
        );

        return 0;
    }

    context->alias_count++;

    return 1;
}


static SemanticModule *semantic_lookup_alias(
    SemanticContext *context,
    const char *alias
)
{
    if (context == NULL ||
        alias == NULL)
        return NULL;

    SemanticScope *scope =
        context->current_scope;

    while (scope != NULL) {

        for (int i = 0;
             i < context->alias_count;
             i++) {

            SemanticAlias *entry =
                &context->aliases[i];

            if (entry->owner == scope &&
                entry->name != NULL &&
                strcmp(entry->name, alias) == 0)
                return entry->module;
        }

        scope = scope->parent;
    }

    return NULL;
}


static int semantic_register_import(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    const char *module_name =
        node->data.import_decl.module;

    if (module_name == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_MODULE_NOT_FOUND,
            node->line,
            node->column,
            "modulo invalido"
        );

        return 0;
    }

    SemanticModule *module =
        semantic_register_module(
            context,
            module_name,
            &node->data.import_decl.declarations,
            node->line,
            node->column
        );

    if (module == NULL)
        return 0;

    const char *alias =
        node->data.import_decl.alias != NULL
            ? node->data.import_decl.alias
            : module_name;

    return semantic_register_alias(
        context,
        alias,
        module,
        node->line,
        node->column
    );
}


static int semantic_register_from_import(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    const char *module_name =
        node->data.from_import.module;

    if (module_name == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_MODULE_NOT_FOUND,
            node->line,
            node->column,
            "modulo invalido"
        );

        return 0;
    }

    SemanticModule *module =
        semantic_register_module(
            context,
            module_name,
            &node->data.from_import.declarations,
            node->line,
            node->column
        );

    if (module == NULL)
        return 0;

    int valid = 1;

    for (int i = 0;
         i < node->data.from_import.names.count;
         i++)
    {
        ASTNode *name_node =
            node->data.from_import.names.items[i];

        if (name_node == NULL ||
            name_node->kind != AST_IDENTIFIER)
            continue;

        const char *function_name =
            name_node->data.identifier.name;

        if (function_name == NULL)
            continue;

        if (semantic_lookup_current_scope(
                context,
                function_name
            ) != NULL) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_DUPLICATE_NAME,
                name_node->line,
                name_node->column,
                "el nombre ya esta declarado"
            );

            valid = 0;
            continue;
        }

        ASTNode *declaration =
            semantic_module_find_function(
                module,
                function_name
            );

        if (declaration == NULL) {

            char message[512];

            snprintf(
                message,
                sizeof(message),
                "el modulo '%s' no tiene la funcion '%s'",
                module_name,
                function_name
            );

            semantic_set_error_suggesting(
                context,
                SEMANTIC_ERROR_IMPORT_NOT_FOUND,
                name_node->line,
                name_node->column,
                message,
                semantic_suggest_module_fn(
                    module,
                    function_name
                )
            );

            valid = 0;
            continue;
        }

        if (function_name[0] == '_') {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_PRIVATE_ACCESS,
                name_node->line,
                name_node->column,
                "only public functions can be imported"
            );

            valid = 0;
            continue;
        }

        if (!semantic_declare_function_symbol(
                context,
                declaration
            )) {

            valid = 0;
        }
        else if (!semantic_module_add_exported(
                     module,
                     function_name
                 )) {

            valid = 0;
        }
    }

    return valid;
}


/*
 * Tipos que pueden cruzar con C en una función externa:
 *
 *     int       -> int64_t
 *     float   -> double
 *     string    -> char *
 *     bool      -> uint8_t
 *     char  -> char
 *
 * Sin listas ni maps: E++ no tiene estructuras ni
 * punteros. NULL (void) se acepta como retorno.
 */
static int semantic_type_crosses_c(
    SemanticContext *context,
    const Type *type
)
{
    if (type == NULL)
        return 1;

    switch (type->kind) {

        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_SIZE:
        case TYPE_OPAQUE:
        case TYPE_CALLBACK:
        case TYPE_BUFFER:
            return 1;

        case TYPE_STRUCT:
            /* Solo structs POD (campos escalares FFI). */
            return semantic_struct_is_pod(context, type);

        default:
            return 0;
    }
}


/*
 * 1 si el tipo es un entero FFI de ancho fijo (solo
 * disponible en firmas `external`).
 */
static int semantic_type_is_ffi_integer(const Type *type)
{
    if (type == NULL)
        return 0;

    switch (type->kind) {

        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_SIZE:
            return 1;

        default:
            return 0;
    }
}


/*
 * 1 si el tipo solo puede aparecer en firmas externas
 * (tipos FFI de ancho fijo y callback). `opaque` no
 * cuenta: se puede usar en variables como handle.
 * `void` tampoco: se maneja como return_type NULL.
 */
static int semantic_type_is_ffi_only(const Type *type)
{
    if (type == NULL)
        return 0;

    if (semantic_type_is_ffi_integer(type))
        return 1;

    return type->kind == TYPE_CALLBACK ||
           type->kind == TYPE_BUFFER;
}


/*
 * 1 si el struct es POD para FFI: todos sus campos son
 * escalares FFI (int, float, bool, char, i8..u64, size).
 * Sin string, sin structs anidados, sin list/map.
 */
static int semantic_struct_is_pod(
    SemanticContext *context,
    const Type *type
)
{
    if (type == NULL ||
        type->kind != TYPE_STRUCT ||
        type->struct_name == NULL)
        return 0;

    SemanticSymbol *symbol =
        semantic_lookup_struct(
            context,
            type->struct_name
        );

    if (symbol == NULL ||
        symbol->struct_decl == NULL)
        return 0;

    ASTNodeList *fields =
        &symbol->struct_decl->data.struct_decl.fields;

    if (fields->count == 0)
        return 0;

    for (int i = 0; i < fields->count; i++) {

        ASTNode *field = fields->items[i];

        if (field == NULL ||
            field->data.var_decl.type == NULL)
            return 0;

        const Type *field_type =
            field->data.var_decl.type;

        /*
         * Solo escalares FFI: int, float, bool, char,
         * i8..u64, size. Sin string, structs, list, map.
         */
        switch (field_type->kind) {

            case TYPE_INT:
            case TYPE_FLOAT:
            case TYPE_BOOL:
            case TYPE_CHAR:
            case TYPE_I8:
            case TYPE_I16:
            case TYPE_I32:
            case TYPE_U8:
            case TYPE_U16:
            case TYPE_U32:
            case TYPE_U64:
            case TYPE_SIZE:
                break;

            default:
                return 0;
        }
    }

    return 1;
}


/*
 * 1 si el tipo es un entero con signo (i8/i16/i32) o
 * `int` (i64 con signo).
 */
static int semantic_type_is_signed_integer(const Type *type)
{
    if (type == NULL)
        return 0;

    switch (type->kind) {

        case TYPE_INT:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
            return 1;

        default:
            return 0;
    }
}


/*
 * 1 si un tipo entero FFI es compatible con `int` (o
 * viceversa): ancho fijo ↔ int se promueve/estrecha
 * de forma segura en la frontera FFI.
 */
static int semantic_ffi_integer_compatible(
    const Type *expected,
    const Type *actual
)
{
    if (expected == NULL || actual == NULL)
        return 0;

    int expected_ffi =
        semantic_type_is_ffi_integer(expected);
    int actual_ffi =
        semantic_type_is_ffi_integer(actual);

    if (expected_ffi && actual->kind == TYPE_INT)
        return 1;

    if (expected->kind == TYPE_INT && actual_ffi)
        return 1;

    /* FFI ↔ FFI solo si es el mismo tipo */
    if (expected_ffi && actual_ffi)
        return expected->kind == actual->kind;

    return 0;
}


/*
 * Valida que todos los tipos de la firma de un callback
 * sean FFI seguros (escalares u opaque, recursivo).
 * Devuelve 1 si es válida.
 */
static int semantic_validate_callback_signature(
    SemanticContext *context,
    const Type *callback,
    int line,
    int column
)
{
    if (callback == NULL ||
        callback->kind != TYPE_CALLBACK)
        return 0;

    for (int i = 0;
         i < callback->parameter_count;
         i++) {

        const Type *param =
            callback->parameter_types[i];

        if (param == NULL)
            return 0;

        if (param->kind == TYPE_CALLBACK ||
            param->kind == TYPE_LIST ||
            param->kind == TYPE_MAP ||
            param->kind == TYPE_TASK ||
            param->kind == TYPE_STRUCT) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                line,
                column,
                "los parametros de un callback deben ser "
                "int, float, string, bool, char, opaque "
                "o un entero FFI (i8..u64, size)"
            );

            return 0;
        }
    }

    const Type *ret = callback->return_type;

    if (ret != NULL &&
        (ret->kind == TYPE_CALLBACK ||
         ret->kind == TYPE_LIST ||
         ret->kind == TYPE_MAP ||
         ret->kind == TYPE_TASK ||
         ret->kind == TYPE_STRUCT)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            line,
            column,
            "el retorno de un callback debe ser int, "
            "float, string, bool, char, opaque, un entero "
            "FFI (i8..u64, size) o void"
        );

        return 0;
    }

    return 1;
}


/*
 * Valida la firma de una función externa. Devuelve 1 si
 * la firma es válida (no hay cuerpo que analizar).
 */
static int semantic_check_external_signature(
    SemanticContext *context,
    ASTNode *node
)
{
    int valid = 1;

    ASTParameterList *parameters =
        &node->data.function_decl.parameters;

    for (int i = 0; i < parameters->count; i++) {

        const Type *param_type =
            parameters->items[i]->type;

        if (param_type != NULL &&
            param_type->kind == TYPE_CALLBACK) {

            if (!semantic_validate_callback_signature(
                    context,
                    param_type,
                    node->line,
                    node->column
                )) {
                valid = 0;
            }

            continue;
        }

        if (semantic_type_crosses_c(context, param_type))
            continue;

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "los parametros de una funcion externa deben "
            "ser int, float, string, bool, char, opaque, "
            "callback, buffer, i8, i16, i32, u8, u16, u32, "
            "u64, size o un struct POD"
        );

        break;
    }

    const Type *return_type =
        node->data.function_decl.return_type;

    if (return_type != NULL &&
        return_type->kind == TYPE_CALLBACK) {

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "una funcion externa no puede devolver un "
            "callback; use opaque si necesita un handle"
        );

    } else if (return_type != NULL &&
               return_type->kind == TYPE_BUFFER) {

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "una funcion externa no puede devolver un "
            "buffer; use list<T> si necesita devolver "
            "una coleccion"
        );

    } else if (!semantic_type_crosses_c(context, return_type)) {

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "el retorno de una funcion externa debe ser "
            "int, float, string, bool, char, opaque, i8, "
            "i16, i32, u8, u16, u32, u64, size, void "
            "o un struct POD"
        );
    }

    /*
     * owned / static solo tiene sentido con retorno string.
     */
    if ((node->data.function_decl.external_string_owned ||
         node->data.function_decl.external_string_static) &&
        (return_type == NULL ||
         return_type->kind != TYPE_STRING)) {

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "owned y static solo se pueden usar con un "
            "retorno de tipo string"
        );
    }

    if (node->data.function_decl.external_string_owned &&
        node->data.function_decl.external_string_static) {

        valid = 0;

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "owned y static son mutuamente excluyentes"
        );
    }

    return valid;
}


static int semantic_analyze_registered_function(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL ||
        node->kind != AST_FUNCTION_DECL)
        return 0;

    SemanticSymbol *symbol =
        semantic_lookup_current_scope(
            context,
            node->data.function_decl.name
        );

    if (symbol == NULL ||
        symbol->function == NULL)
        return 0;

    if (!semantic_validate_type(
            context,
            symbol->function->return_type,
            node->line,
            node->column
        ))
        return 0;

    for (int i = 0;
         i < symbol->function->parameter_count;
         i++) {
        if (!semantic_validate_type(
                context,
                symbol->function->parameters[i]->type,
                node->line,
                node->column
            ))
            return 0;
    }

    /*
     * Función externa (C): solo se valida la firma; no
     * hay cuerpo que analizar.
     */
    if (node->data.function_decl.external_path != NULL) {

        return semantic_check_external_signature(
            context,
            node
        );
    }

    /*
     * Función normal: los tipos FFI de ancho fijo no
     * están permitidos en su firma.
     */
    if (semantic_type_is_ffi_only(
            symbol->function->return_type
        )) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_TYPE,
            node->line,
            node->column,
            "los tipos i8, i16, i32, u8, u16, u32, u64 y "
            "size solo estan disponibles en firmas de "
            "funciones externas (external)"
        );

        return 0;
    }

    for (int i = 0;
         i < symbol->function->parameter_count;
         i++) {

        if (semantic_type_is_ffi_only(
                symbol->function->parameters[i]->type
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                node->line,
                node->column,
                "los tipos i8, i16, i32, u8, u16, u32, "
                "u64 y size solo estan disponibles en "
                "firmas de funciones externas (external)"
            );

            return 0;
        }
    }

    SemanticFunction *previous =
        context->current_function;

    context->current_function =
        symbol->function;

    context->function_depth++;

    if (!semantic_enter_scope(context)) {

        context->function_depth--;
        context->current_function =
            previous;

        return 0;
    }

    int valid = 1;

    /*
     * Parámetros.
     */
    for (int p = 0;
         p < symbol->function->parameter_count;
         p++) {

        SemanticParameter *parameter =
            symbol->function->parameters[p];

        SemanticSymbol *parameter_symbol =
            calloc(
                1,
                sizeof(SemanticSymbol)
            );

        if (parameter_symbol == NULL) {

            valid = 0;
            continue;
        }

        parameter_symbol->name =
            semantic_strdup(
                parameter->name
            );

        parameter_symbol->type =
            semantic_copy_type(
                parameter->type
            );

        parameter_symbol->kind =
            SYMBOL_PARAMETER;

        parameter_symbol->is_private =
            parameter->is_private;

        parameter_symbol->is_initialized =
            1;

        parameter_symbol->line =
            node->line;

        parameter_symbol->column =
            node->column;

        if (!semantic_declare_symbol(
                context,
                parameter_symbol
            )) {

            semantic_symbol_destroy(
                parameter_symbol
            );

            valid = 0;
        }
    }

    if (!semantic_analyze_block(
            context,
            node->data.function_decl.body
        )) {

        valid = 0;
    }

    semantic_leave_scope(context);

    context->function_depth--;

    context->current_function =
        previous;

    return valid;
}


static int semantic_analyze_module_declarations(
    SemanticContext *context,
    ASTNode *import_node
)
{
    if (context == NULL ||
        import_node == NULL)
        return 0;

    const char *module_name = NULL;
    ASTNodeList *declarations = NULL;

    if (import_node->kind == AST_IMPORT) {

        module_name =
            import_node->data.import_decl.module;

        declarations =
            &import_node
                ->data.import_decl.declarations;
    }
    else if (import_node->kind ==
             AST_FROM_IMPORT)
    {
        module_name =
            import_node->data.from_import.module;

        declarations =
            &import_node
                ->data.from_import.declarations;
    }
    else {
        return 0;
    }

    SemanticModule *module =
        semantic_find_module(
            context,
            module_name
        );

    if (module == NULL ||
        module->scope == NULL)
        return 1;

    if (declarations == NULL ||
        declarations->count == 0)
        return 1;

    SemanticScope *previous =
        context->current_scope;

    context->current_scope = module->scope;

    int valid = 1;

    for (int i = 0; i < declarations->count; i++) {
        ASTNode *node = declarations->items[i];

        if (node != NULL &&
            node->kind == AST_STRUCT_DECL &&
            !semantic_analyze_struct(context, node))
            valid = 0;
    }

    for (int i = 0;
         i < declarations->count;
         i++) {

        ASTNode *node =
            declarations->items[i];

        if (node == NULL)
            continue;

        if (node->kind == AST_STRUCT_DECL)
            continue;

        if (node->kind ==
            AST_FUNCTION_DECL)
        {
            if (!semantic_analyze_registered_function(
                    context,
                    node
                )) {

                valid = 0;
            }
        }
        else if (node->kind == AST_IMPORT ||
                 node->kind == AST_FROM_IMPORT)
        {
            if (!semantic_analyze_module_declarations(
                    context,
                    node
                )) {

                valid = 0;
            }
        }
        else {
            if (!semantic_analyze_declaration(
                    context,
                    node
                )) {

                valid = 0;
            }
        }
    }

    context->current_scope = previous;

    return valid;
}


/*
 * El namespace de nombres es plano (un solo módulo LLVM):
 * dos funciones con el mismo nombre no pueden emitirse.
 *
 * Se comprueba al final, cuando todos los registros
 * (principal, módulos y from-import) ya existen.
 */
static int semantic_check_module_collisions(
    SemanticContext *context,
    ASTNodeList *declarations
)
{
    if (context == NULL)
        return 1;

    (void) declarations;

    int valid = 1;

    for (int m = 0;
         m < context->module_count;
         m++) {

        SemanticModule *module =
            context->modules[m];

        if (module == NULL ||
            module->declarations == NULL)
            continue;

        for (int i = 0;
             i < module->declarations->count;
             i++) {

            ASTNode *node =
                module->declarations->items[i];

            if (node == NULL ||
                node->kind != AST_FUNCTION_DECL)
                continue;

            const char *name =
                node->data.function_decl.name;

            if (name == NULL)
                continue;

            int duplicate = 0;

            /*
             * ¿Ya existe en el scope global (función del
             * principal o exportada por otro módulo) sin
             * ser exportación propia?
             */
            if (semantic_lookup_current_scope(
                    context,
                    name
                ) != NULL &&
                !semantic_module_exported(
                    module,
                    name))
            {
                duplicate = 1;
            }

            /*
             * ¿Otro módulo ya declaró esa función?
             */
            for (int k = 0;
                 k < m && !duplicate;
                 k++) {

                SemanticModule *other =
                    context->modules[k];

                if (other == NULL ||
                    other->declarations == NULL)
                    continue;

                if (semantic_module_find_function(
                        other,
                        name) != NULL) {
                    duplicate = 1;
                }
            }

            if (duplicate) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_DUPLICATE_NAME,
                    node->line,
                    node->column,
                    "la funcion ya esta declarada"
                );

                valid = 0;
            }
        }
    }

    return valid;
}


/* =========================================================
 * IDENTIFICADOR
 * ========================================================= */

Type *semantic_analyze_identifier(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return NULL;

    SemanticSymbol *symbol =
        semantic_lookup_symbol(
            context,
            node->data.identifier.name
        );

    if (symbol == NULL) {

        semantic_set_error_suggesting(
            context,
            SEMANTIC_ERROR_UNDEFINED_NAME,
            node->line,
            node->column,
            "name not defined",
            semantic_suggest_name(
                context,
                node->data.identifier.name,
                0
            )
        );

        return NULL;
    }

    if (!semantic_check_visibility(
            context,
            symbol
        )) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_PRIVATE_ACCESS,
            node->line,
            node->column,
            "acceso a un nombre privado"
        );

        return NULL;
    }

    symbol->is_used = 1;

    if (!symbol->is_initialized) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_VARIABLE_NOT_INITIALIZED,
            node->line,
            node->column,
            "la variable no esta inicializada"
        );

        return NULL;
    }

    if (symbol->kind == SYMBOL_STRUCT) {
        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "un struct solo se puede usar como constructor"
        );
        return NULL;
    }

    if (symbol->kind == SYMBOL_FUNCTION &&
        symbol->function != NULL)
        return semantic_type_from_function(symbol->function);

    return semantic_copy_type(
        symbol->type
    );
}


/* =========================================================
 * BINARY
 * ========================================================= */

Type *semantic_analyze_binary(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *left =
        semantic_analyze_expression(
            context,
            node->data.binary.left
        );

    Type *right =
        semantic_analyze_expression(
            context,
            node->data.binary.right
        );

    if (left == NULL || right == NULL) {

        semantic_destroy_type(left);
        semantic_destroy_type(right);

        return NULL;
    }

    Type *result =
        semantic_binary_result_type(
            node->data.binary.operator,
            left,
            right
        );

    if (result == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "invalid operation for these types"
        );
    }

    semantic_destroy_type(left);
    semantic_destroy_type(right);

    return result;
}


/* =========================================================
 * UNARY
 * ========================================================= */

Type *semantic_analyze_unary(
    SemanticContext *context,
    ASTNode *node
)
{
    ASTOperator operator =
        node->data.unary.operator;

    if (operator == AST_OP_INCREMENT ||
        operator == AST_OP_DECREMENT) {
        ASTNode *operand_node =
            node->data.unary.operand;

        if (operand_node == NULL ||
            operand_node->kind != AST_IDENTIFIER) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_ASSIGNMENT,
                node->line,
                node->column,
                "++ y -- requieren una variable mutable"
            );
            return NULL;
        }

        SemanticSymbol *symbol =
            semantic_lookup_symbol(
                context,
                operand_node->data.identifier.name
            );

        if (symbol == NULL) {
            semantic_set_error_suggest(
                context,
                SEMANTIC_ERROR_UNDEFINED_NAME,
                operand_node->line,
                operand_node->column,
                "variable no definida",
                semantic_suggest_name(
                    context,
                    operand_node->data.identifier.name,
                    0
                )
            );
            return NULL;
        }

        if (symbol->is_const ||
            symbol->kind != SYMBOL_VARIABLE) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_ASSIGN_TO_CONST,
                operand_node->line,
                operand_node->column,
                "++ y -- no pueden modificar una constante"
            );
            return NULL;
        }

        if (symbol->type == NULL ||
            symbol->type->kind != TYPE_INT) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_OPERATION,
                operand_node->line,
                operand_node->column,
                "++ y -- solo admiten variables int"
            );
            return NULL;
        }

        return semantic_create_basic_type(TYPE_INT);
    }

    Type *operand =
        semantic_analyze_expression(
            context,
            node->data.unary.operand
        );

    if (operand == NULL)
        return NULL;

    Type *result =
        semantic_unary_result_type(
            node->data.unary.operator,
            operand
        );

    if (result == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "operador unario invalido"
        );
    }

    semantic_destroy_type(operand);

    return result;
}


/* =========================================================
 * AWAIT (§52)
 * ========================================================= */

static Type *semantic_analyze_await(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return NULL;

    Type *operand =
        semantic_analyze_expression(
            context,
            node->data.await_expr.operand
        );

    if (operand == NULL)
        return NULL;

    if (operand->kind != TYPE_TASK) {

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "await requiere una tarea task<T> "
            "pero se encontro %s",
            semantic_type_name(operand)
        );

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            message
        );

        semantic_destroy_type(operand);
        return NULL;
    }

    /*
     * El resultado de await es el tipo de la tarea.
     * task<null> (async fn sin -> tipo) produce null.
     */
    Type *result = operand->element_type;
    operand->element_type = NULL;

    semantic_destroy_type(operand);

    if (result == NULL)
        return semantic_create_basic_type(TYPE_NULL);

    return result;
}


/* =========================================================
 * ASIGNACIÓN
 * ========================================================= */

Type *semantic_analyze_assignment(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return NULL;

    ASTNode *target =
        node->data.assignment.target;

    if (target == NULL)
        return NULL;

    Type *target_type = NULL;

    if (target->kind == AST_IDENTIFIER) {

        SemanticSymbol *symbol =
            semantic_lookup_symbol(
                context,
                target->data.identifier.name
            );

        if (symbol == NULL) {

            semantic_set_error_suggesting(
                context,
                SEMANTIC_ERROR_UNDEFINED_NAME,
                target->line,
                target->column,
                "variable no definida",
                semantic_suggest_name(
                    context,
                    target->data.identifier.name,
                    0
                )
            );

            return NULL;
        }

        if (symbol->is_const ||
            symbol->kind == SYMBOL_CONSTANT ||
            symbol->kind == SYMBOL_FUNCTION) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ASSIGN_TO_CONST,
                target->line,
                target->column,
                "cannot assign to a constant"
            );

            return NULL;
        }

        target_type =
            semantic_copy_type(
                symbol->type
            );

    } else if (
        target->kind == AST_INDEX
    ) {

        target_type =
            semantic_analyze_index(
                context,
                target
            );

    } else if (
        target->kind == AST_MEMBER
    ) {

        target_type =
            semantic_analyze_member(
                context,
                target
            );

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_ASSIGNMENT,
            target->line,
            target->column,
            "el destino no puede recibir una asignacion"
        );

        return NULL;
    }

    if (target_type == NULL)
        return NULL;

    semantic_destroy_type(target->expr_type);
    target->expr_type =
        semantic_copy_type(target_type);

    if (target->expr_type == NULL) {
        semantic_destroy_type(target_type);
        return NULL;
    }

    int handled = 0;

    Type *value_type =
        semantic_analyze_empty_literal(
            context,
            node->data.assignment.value,
            target_type,
            &handled
        );

    if (!handled) {

        value_type =
            semantic_analyze_expression(
                context,
                node->data.assignment.value
            );
    }

    if (value_type == NULL) {

        semantic_destroy_type(target_type);
        return NULL;
    }

    if (node->data.assignment.operator ==
        AST_OP_ASSIGN) {

        if (!semantic_type_compatible(
                target_type,
                value_type
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_TYPE_MISMATCH,
                node->line,
                node->column,
                "tipo incompatible en asignacion"
            );

            semantic_destroy_type(target_type);
            semantic_destroy_type(value_type);

            return NULL;
        }

    } else {

        Type *result =
            semantic_binary_result_type(
                node->data.assignment.operator ==
                    AST_OP_ADD_ASSIGN
                    ? AST_OP_ADD
                    : node->data.assignment.operator ==
                        AST_OP_SUB_ASSIGN
                        ? AST_OP_SUB
                        : node->data.assignment.operator ==
                            AST_OP_MUL_ASSIGN
                            ? AST_OP_MUL
                            : AST_OP_DIV,
                target_type,
                value_type
            );

        if (result == NULL ||
            !semantic_type_compatible(
                target_type,
                result
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_ASSIGNMENT,
                node->line,
                node->column,
                "operacion de asignacion invalida"
            );

            semantic_destroy_type(result);
            semantic_destroy_type(target_type);
            semantic_destroy_type(value_type);

            return NULL;
        }

        semantic_destroy_type(result);
    }

    semantic_destroy_type(value_type);

    return target_type;
}


/* =========================================================
 * CALL
 * ========================================================= */

int semantic_check_arguments(
    SemanticContext *context,
    SemanticFunction *function,
    ASTNodeList *arguments
)
{
    if (function == NULL ||
        arguments == NULL)
        return 0;

    /*
     * Variadic: se permiten mas argumentos que
     * parametros fijos.
     */
    if (!function->is_variadic &&
        function->parameter_count !=
        arguments->count) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_ARGUMENT_COUNT,
            0,
            0,
            "cantidad incorrecta de argumentos"
        );

        return 0;
    }

    if (function->is_variadic &&
        arguments->count < function->parameter_count) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_ARGUMENT_COUNT,
            0,
            0,
            "faltan argumentos en la llamada variadic"
        );

        return 0;
    }

    int valid = 1;

    int check_count =
        function->parameter_count < arguments->count
            ? function->parameter_count
            : arguments->count;

    for (int i = 0;
         i < check_count;
         i++) {

        Type *argument_type =
            semantic_analyze_expression(
                context,
                arguments->items[i]
            );

        if (argument_type == NULL) {

            valid = 0;
            continue;
        }

        Type *expected =
            function->parameters[i]->type;

        if (!semantic_type_compatible(
                expected,
                argument_type
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                arguments->items[i]->line,
                arguments->items[i]->column,
                "incorrect argument type"
            );

            valid = 0;
        }

        semantic_destroy_type(argument_type);
    }

    /*
     * Argumentos extra en variadic: solo se analizan,
     * sin comprobar tipo contra firma.
     */
    if (function->is_variadic) {

        for (int i = check_count;
             i < arguments->count;
             i++) {

            Type *argument_type =
                semantic_analyze_expression(
                    context,
                    arguments->items[i]
                );

            if (argument_type == NULL)
                valid = 0;

            semantic_destroy_type(argument_type);
        }
    }

    return valid;
}


/* =========================================================
 * FUNCIONES INCORPORADAS DEL RUNTIME
 *
 * print() / input() / len() no las declara el usuario:
 * se resuelven contra el runtime de E++ (runtime.h).
 *
 * Solo se admiten los tipos cuya semántica está definida:
 *
 *   print : int, float, string, bool, char
 *   input : sin argumentos, devuelve string
 *   len   : string, list<T>
 *
 * Para el resto de tipos no se inventa comportamiento.
 * ======================================================= */

static Type *semantic_analyze_builtin_call(
    SemanticContext *context,
    ASTNode *node,
    int *handled
)
{
    *handled = 0;

    if (context == NULL ||
        node == NULL ||
        node->data.call.callee == NULL ||
        node->data.call.callee->kind != AST_IDENTIFIER) {

        return NULL;
    }

    const char *name =
        node->data.call.callee->data.identifier.name;

    if (name == NULL)
        return NULL;

    int count = node->data.call.arguments.count;


    /* ---- print(valor) ---- */

    if (strcmp(name, "print") == 0) {

        *handled = 1;

        if (count == 0) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "print() takes at least 1 argument"
            );
            return NULL;
        }

        for (int i = 0; i < count; i++) {
            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[i]
                );

            if (argument == NULL)
                return NULL;

            int supported =
                semantic_type_is_printable(
                    context,
                    argument
                );

            if (!supported) {
                const char *message =
                    argument->kind == TYPE_STRUCT
                        ? "print() no puede mostrar structs con campos list, map o null"
                        : "print() only supports int, float, string, "
                          "bool, char or struct";

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_TYPE,
                    node->data.call.arguments.items[i]->line,
                    node->data.call.arguments.items[i]->column,
                    message
                );
                semantic_destroy_type(argument);
                return NULL;
            }

            semantic_destroy_type(argument);
        }

        /* print() no devuelve valor. */
        return NULL;
    }


    /* ---- str(valor) ---- */

    if (strcmp(name, "str") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "str() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        int supported =
            argument->kind == TYPE_INT ||
            argument->kind == TYPE_FLOAT ||
            argument->kind == TYPE_STRING ||
            argument->kind == TYPE_BOOL ||
            argument->kind == TYPE_CHAR;

        if (!supported) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "str() only supports int, float, string, "
                "bool or char"
            );

            semantic_destroy_type(argument);
            return NULL;
        }

        semantic_destroy_type(argument);

        /* str() devuelve string. */
        return semantic_create_basic_type(TYPE_STRING);
    }


    /* ---- range(inicio, fin) -> list<int> ---- */

    if (strcmp(name, "range") == 0) {

        *handled = 1;

        if (count != 2) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "range() takes exactly 2 arguments"
            );

            return NULL;
        }

        for (int i = 0; i < 2; i++) {

            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[i]
                );

            if (argument == NULL)
                return NULL;

            int supported = argument->kind == TYPE_INT;

            semantic_destroy_type(argument);

            if (!supported) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_TYPE,
                    node->line,
                    node->column,
                    "range() only accepts integers (int)"
                );

                return NULL;
            }
        }

        return semantic_create_list_type(
            semantic_create_basic_type(TYPE_INT)
        );
    }


    /* ---- divmod(a, b) -> list<int> ---- */

    if (strcmp(name, "divmod") == 0) {

        *handled = 1;

        if (count != 2) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "divmod() takes exactly 2 arguments"
            );

            return NULL;
        }

        for (int i = 0; i < 2; i++) {

            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[i]
                );

            if (argument == NULL)
                return NULL;

            int supported = argument->kind == TYPE_INT;

            semantic_destroy_type(argument);

            if (!supported) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_TYPE,
                    node->line,
                    node->column,
                    "divmod() only accepts integers (int)"
                );

                return NULL;
            }
        }

        return semantic_create_list_type(
            semantic_create_basic_type(TYPE_INT)
        );
    }


    /* ---- sorted(list<int>) -> list<int> ---- */

    if (strcmp(name, "sorted") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "sorted() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        int supported =
            argument->kind == TYPE_LIST &&
            argument->element_type != NULL &&
            argument->element_type->kind == TYPE_INT;

        semantic_destroy_type(argument);

        if (!supported) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "sorted() only accepts list<int>"
            );

            return NULL;
        }

        return semantic_create_list_type(
            semantic_create_basic_type(TYPE_INT)
        );
    }


    /* ---- reversed(list<T>) -> list<T> ---- */

    if (strcmp(name, "reversed") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "reversed() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        if (argument->kind != TYPE_LIST) {

            semantic_destroy_type(argument);

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "reversed() only accepts lists"
            );

            return NULL;
        }

        /* preservar el tipo de elemento de la entrada */
        TypeKind kind =
            argument->element_type != NULL
                ? argument->element_type->kind
                : TYPE_INT;

        semantic_destroy_type(argument);

        return semantic_create_list_type(
            semantic_create_basic_type(kind)
        );
    }


    /* ---- all(list<bool>) -> bool ---- */

    if (strcmp(name, "all") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "all() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        int supported =
            argument->kind == TYPE_LIST &&
            argument->element_type != NULL &&
            argument->element_type->kind == TYPE_BOOL;

        semantic_destroy_type(argument);

        if (!supported) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "all() only accepts list<bool>"
            );

            return NULL;
        }

        return semantic_create_basic_type(TYPE_BOOL);
    }


    /* ---- any(list<bool>) -> bool ---- */

    if (strcmp(name, "any") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "any() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        int supported =
            argument->kind == TYPE_LIST &&
            argument->element_type != NULL &&
            argument->element_type->kind == TYPE_BOOL;

        semantic_destroy_type(argument);

        if (!supported) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "any() only accepts list<bool>"
            );

            return NULL;
        }

        return semantic_create_basic_type(TYPE_BOOL);
    }


    /* ---- input([prompt]) ---- */

    if (strcmp(name, "input") == 0) {

        *handled = 1;

        if (count > 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "input() acepta cero o un argumento string"
            );

            return NULL;
        }

        if (count == 1) {
            Type *prompt_type =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[0]
                );

            if (prompt_type == NULL)
                return NULL;

            int is_string = prompt_type->kind == TYPE_STRING;
            semantic_destroy_type(prompt_type);

            if (!is_string) {
                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_TYPE,
                    node->data.call.arguments.items[0]->line,
                    node->data.call.arguments.items[0]->column,
                    "input() prompt debe ser string"
                );
                return NULL;
            }
        }

        return semantic_create_basic_type(TYPE_STRING);
    }


    /* ---- len(valor) ---- */

    if (strcmp(name, "len") == 0) {

        *handled = 1;

        if (count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "len() takes exactly 1 argument"
            );

            return NULL;
        }

        Type *argument =
            semantic_analyze_expression(
                context,
                node->data.call.arguments.items[0]
            );

        if (argument == NULL)
            return NULL;

        Type *result = NULL;

        if (argument->kind == TYPE_STRING) {

            result =
                semantic_create_basic_type(TYPE_INT);

        } else if (argument->kind == TYPE_LIST) {

            /*
             * len(list<T>) está definido en el spec §11 y el
             * runtime ya lo implementa (rivo_list_len).
             */
            result =
                semantic_create_basic_type(TYPE_INT);

        } else if (argument->kind == TYPE_MAP) {

            /*
             * len(map<K,V>): decisión de diseño 2026-10-06.
             * El spec §12 no lo menciona, pero la longitud
             * es coherente con string y list<T> y el runtime
             * ya la mantiene para el for (rivo_map_len).
             */
            result =
                semantic_create_basic_type(TYPE_INT);

        } else {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_TYPE,
                node->line,
                node->column,
                "la longitud solo esta definida para "
                "string, list<T> y map<K,V>"
            );
        }

        semantic_destroy_type(argument);

        return result;
    }


    /*
     * Funciones incorporadas nuevas (fase 2.1): la tabla
     * compartida con el codegen vive en include/builtins.h.
     */
    const RivoBuiltin *builtin =
        rivo_builtin_find(name);

    if (builtin != NULL) {

        *handled = 1;

        if (count != builtin->argument_count) {

            char message[160];

            snprintf(
                message,
                sizeof(message),
                "%s() takes exactly %d %s",
                name,
                builtin->argument_count,
                builtin->argument_count == 1
                    ? "argument"
                    : "arguments"
            );

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                message
            );

            return NULL;
        }

        int valid = 1;

        for (int i = 0; i < count; i++) {

            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[i]
                );

            if (argument == NULL)
                return NULL;

            int compatible = 0;

            if (builtin->arguments[i] == TYPE_LIST) {

                /* join(...): solo admite list<string>. */
                compatible =
                    argument->kind == TYPE_LIST &&
                    argument->element_type != NULL &&
                    argument->element_type->kind ==
                        TYPE_STRING;

            } else {

                Type *expected =
                    semantic_create_basic_type(
                        builtin->arguments[i]
                    );

                compatible =
                    expected != NULL &&
                    semantic_type_compatible(
                        expected,
                        argument
                    );

                semantic_destroy_type(expected);
            }

            if (!compatible) {

                valid = 0;

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_TYPE,
                    node->data.call.arguments.items[i]->line,
                    node->data.call.arguments.items[i]->column,
                    "incorrect argument type"
                );
            }

            semantic_destroy_type(argument);
        }

        if (!valid)
            return NULL;

        if (builtin->result == TYPE_NULL)
            return NULL;

        /*
         * split() es la única que devuelve list<string>.
         */
        if (builtin->result == TYPE_LIST) {

            return semantic_create_list_type(
                semantic_create_basic_type(TYPE_STRING)
            );
        }

        return semantic_create_basic_type(
            builtin->result
        );
    }

    return NULL;
}


/*
 * Llamada a un método de lista: lista.add(x),
 * lista.remove(x), lista.len() (claude.md §11).
 *
 * Se llega aquí cuando el callee es un miembro pero el
 * objeto no es un alias de módulo.
 *
 * `handled` indica si la llamada ha sido reconocida (para
 * bien o para mal): si es 0 el llamante debe continuar con
 * su mensaje habitual.
 */
static Type *semantic_analyze_method_call(
    SemanticContext *context,
    ASTNode *node,
    int *handled
)
{
    *handled = 0;

    ASTNode *callee = node->data.call.callee;
    ASTNode *object = callee->data.member.object;
    const char *member = callee->data.member.member;

    Type *object_type =
        semantic_analyze_expression(
            context,
            object
        );

    if (object_type == NULL) {
        *handled = 1;
        return NULL;
    }

    if (object_type->kind != TYPE_LIST &&
        object_type->kind != TYPE_MAP) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "el tipo no tiene miembros"
        );

        semantic_destroy_type(object_type);

        *handled = 1;
        return NULL;
    }

    *handled = 1;

    int argument_count =
        node->data.call.arguments.count;

    Type *result = NULL;

    if (object_type->kind == TYPE_MAP) {

        /*
         * mapa.remove(clave) (decisión de diseño
         * 2026-10-06): coherente con lista.remove().
         * mapa.len() (0.3.0): unificado con lista.len().
         */
        if (strcmp(member, "len") == 0) {

            if (argument_count != 0) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_COUNT,
                    node->line,
                    node->column,
                    "mapa.len() no recibe argumentos"
                );

            } else {

                result =
                    semantic_create_basic_type(TYPE_INT);
            }

        } else if (strcmp(member, "remove") != 0) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
                node->line,
                node->column,
                "miembro desconocido de map"
            );

        } else if (argument_count != 1) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "mapa.remove() recibe exactamente 1 argumento"
            );

        } else {

            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[0]
                );

            if (argument != NULL) {

                if (!semantic_type_compatible(
                        object_type->key_type,
                        argument
                    )) {

                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_ARGUMENT_TYPE,
                        node->data.call.arguments.items[0]->line,
                        node->data.call.arguments.items[0]->column,
                        "incorrect argument type"
                    );
                }

                semantic_destroy_type(argument);
            }
        }

    } else if (
        strcmp(member, "len") == 0) {

        if (argument_count != 0) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                "lista.len() no recibe argumentos"
            );

        } else {

            result =
                semantic_create_basic_type(TYPE_INT);
        }

    } else if (
        strcmp(member, "add") == 0 ||
        strcmp(member, "remove") == 0
    ) {

        const char *label =
            strcmp(member, "add") == 0
                ? "add"
                : "remove";

        if (argument_count != 1) {

            char message[160];

            snprintf(
                message,
                sizeof(message),
                "lista.%s() recibe exactamente 1 argumento",
                label
            );

            semantic_set_error(
                context,
                SEMANTIC_ERROR_ARGUMENT_COUNT,
                node->line,
                node->column,
                message
            );

        } else {

            Type *argument =
                semantic_analyze_expression(
                    context,
                    node->data.call.arguments.items[0]
                );

            if (argument != NULL) {

                if (!semantic_type_compatible(
                        object_type->element_type,
                        argument
                    )) {

                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_ARGUMENT_TYPE,
                        node->data.call.arguments.items[0]->line,
                        node->data.call.arguments.items[0]->column,
                        "incorrect argument type"
                    );
                }

                semantic_destroy_type(argument);
            }
        }

        /*
         * add y remove no devuelven valor
         * (claude.md §7: sin tipo de retorno).
         */
        result = NULL;

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
            node->line,
            node->column,
            "miembro desconocido de list"
        );
    }

    semantic_destroy_type(object_type);

    return result;
}


Type *semantic_analyze_call(
    SemanticContext *context,
    ASTNode *node
)
{
    if (node == NULL)
        return NULL;

    if (node->data.call.callee == NULL)
        return NULL;

    node->data.call.is_struct_constructor = 0;

    SemanticFunction *function = NULL;

    int temporary_function = 0;

    if (node->data.call.callee->kind ==
        AST_IDENTIFIER) {

        SemanticSymbol *callee_symbol =
            semantic_lookup_symbol(
                context,
                node->data.call.callee
                    ->data.identifier.name
            );

        if (callee_symbol != NULL &&
            callee_symbol->kind == SYMBOL_STRUCT &&
            callee_symbol->struct_decl != NULL) {
            ASTNodeList *fields =
                &callee_symbol->struct_decl
                    ->data.struct_decl.fields;

            if (fields->count !=
                node->data.call.arguments.count) {
                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_ARGUMENT_COUNT,
                    node->line,
                    node->column,
                    "cantidad incorrecta de campos para el constructor"
                );
                return NULL;
            }

            for (int i = 0; i < fields->count; i++) {
                ASTNode *field = fields->items[i];
                ASTNode *argument =
                    node->data.call.arguments.items[i];

                int handled = 0;
                Type *argument_type =
                    semantic_analyze_empty_literal(
                        context,
                        argument,
                        field->data.var_decl.type,
                        &handled
                    );

                if (!handled) {
                    argument_type =
                        semantic_analyze_expression(
                            context,
                            argument
                        );
                }

                if (argument_type == NULL)
                    return NULL;

                int compatible =
                    semantic_type_compatible(
                        field->data.var_decl.type,
                        argument_type
                    );

                semantic_destroy_type(argument_type);

                if (!compatible) {
                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_ARGUMENT_TYPE,
                        argument->line,
                        argument->column,
                        "tipo incompatible para el campo del struct"
                    );
                    return NULL;
                }
            }

            node->data.call.is_struct_constructor = 1;
            semantic_destroy_type(
                node->data.call.callee->expr_type
            );
            node->data.call.callee->expr_type =
                semantic_copy_type(callee_symbol->type);

            if (node->data.call.callee->expr_type == NULL)
                return NULL;

            return semantic_copy_type(callee_symbol->type);
        }

        function =
            semantic_lookup_function(
                context,
                node->data.call.callee
                    ->data.identifier.name
            );

        if (function == NULL) {
            SemanticSymbol *symbol =
                semantic_lookup_symbol(
                    context,
                    node->data.call.callee
                        ->data.identifier.name
                );

            if (symbol != NULL &&
                symbol->kind != SYMBOL_FUNCTION &&
                symbol->type != NULL &&
                symbol->type->kind == TYPE_FUNCTION) {
                Type *signature = symbol->type;

                node->data.call.callee->expr_type =
                    semantic_copy_type(signature);

                if (signature->parameter_count !=
                    node->data.call.arguments.count) {
                    semantic_set_error(
                        context,
                        SEMANTIC_ERROR_ARGUMENT_COUNT,
                        node->line,
                        node->column,
                        "cantidad incorrecta de argumentos"
                    );
                    return NULL;
                }

                for (int i = 0;
                     i < signature->parameter_count;
                     i++) {
                    Type *argument_type =
                        semantic_analyze_expression(
                            context,
                            node->data.call.arguments.items[i]
                        );

                    if (argument_type == NULL)
                        return NULL;

                    int compatible =
                        semantic_type_compatible(
                            signature->parameter_types[i],
                            argument_type
                        );
                    semantic_destroy_type(argument_type);

                    if (!compatible) {
                        semantic_set_error(
                            context,
                            SEMANTIC_ERROR_ARGUMENT_TYPE,
                            node->data.call.arguments.items[i]->line,
                            node->data.call.arguments.items[i]->column,
                            "incorrect argument type"
                        );
                        return NULL;
                    }
                }

                return semantic_copy_type(signature->return_type);
            }
        }

    } else if (node->data.call.callee->kind ==
               AST_MEMBER) {

        /*
         * Llamada calificada: math.suma(...)
         *
         * El objeto debe ser un alias de módulo visible
         * desde el scope actual.
         */
        ASTNode *object =
            node->data.call.callee
                ->data.member.object;

        SemanticModule *module =
            (object != NULL &&
             object->kind == AST_IDENTIFIER)
                ? semantic_lookup_alias(
                    context,
                    object
                        ->data.identifier.name
                  )
                : NULL;

        if (module == NULL) {

            /*
             * No es un alias de módulo: puede ser un método
             * de lista (add / remove / len).
             */
            int handled = 0;

            Type *method =
                semantic_analyze_method_call(
                    context,
                    node,
                    &handled
                );

            if (handled)
                return method;

            semantic_set_error(
                context,
                SEMANTIC_ERROR_FUNCTION_NOT_FOUND,
                node->line,
                node->column,
                "no se puede llamar a esta expresion"
            );

            return NULL;
        }

        ASTNode *declaration =
            semantic_module_find_function(
                module,
                node->data.call.callee
                    ->data.member.member
            );

        if (declaration == NULL) {

            char message[512];

            snprintf(
                message,
                sizeof(message),
                "el modulo '%s' no tiene la funcion '%s'",
                module->name,
                node->data.call.callee
                    ->data.member.member
                    != NULL
                        ? node->data.call.callee
                              ->data.member.member
                        : "?"
            );

            semantic_set_error_suggesting(
                context,
                SEMANTIC_ERROR_IMPORT_NOT_FOUND,
                node->line,
                node->column,
                message,
                semantic_suggest_module_fn(
                    module,
                    node->data.call.callee
                        ->data.member.member
                )
            );

            return NULL;
        }

        if (declaration
                ->data.function_decl.name[0] ==
            '_')
        {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_PRIVATE_ACCESS,
                node->line,
                node->column,
                "only public functions of a module can be called"
            );

            return NULL;
        }

        function =
            semantic_function_from_ast(
                declaration
            );

        if (function == NULL)
            return NULL;

        temporary_function = 1;

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_FUNCTION_NOT_FOUND,
            node->line,
            node->column,
            "no se puede llamar a esta expresion"
        );

        return NULL;
    }

    if (function == NULL) {

        /*
         * No es una función declarada por el usuario:
         * probamos con las funciones incorporadas del
         * runtime (print / input / len).
         */
        int handled = 0;

        Type *builtin =
            semantic_analyze_builtin_call(
                context,
                node,
                &handled
            );

        if (handled) {
            node->data.call.is_builtin = 1;
            return builtin;
        }

            const char *callee_name =
                node->data.call.callee->kind ==
                    AST_IDENTIFIER
                    ? node->data.call.callee
                          ->data.identifier.name
                    : NULL;

            semantic_set_error_suggesting(
                context,
                SEMANTIC_ERROR_FUNCTION_NOT_FOUND,
                node->data.call.callee->line,
                node->data.call.callee->column,
                "funcion no encontrada",
                callee_name != NULL
                    ? semantic_suggest_name(
                          context,
                          callee_name,
                          1
                      )
                    : NULL
            );

        return NULL;
    }

    if (!semantic_check_arguments(
            context,
            function,
            &node->data.call.arguments
        )) {

        if (temporary_function)
            semantic_function_destroy(function);

        return NULL;
    }

    /*
     * Asignar el tipo de función al callee para que el
     * codegen pueda detectar parámetros buffer<T> y
     * expandirlos en (T*, size).
     */
    {
        int param_count = function->parameter_count;

        Type *callee_type =
            calloc(1, sizeof(Type));

        if (callee_type != NULL) {

            callee_type->kind = TYPE_FUNCTION;
            callee_type->return_type =
                semantic_copy_type(function->return_type);

            if (param_count > 0) {

                callee_type->parameter_types =
                    calloc(
                        param_count,
                        sizeof(Type *)
                    );

                if (callee_type->parameter_types != NULL) {

                    for (int i = 0; i < param_count; i++)
                        callee_type->parameter_types[i] =
                            semantic_copy_type(
                                function->parameters[i]->type
                            );

                    callee_type->parameter_count =
                        param_count;
                }
            }

            semantic_destroy_type(
                node->data.call.callee->expr_type
            );

            node->data.call.callee->expr_type =
                callee_type;
        }
    }

    Type *result =
        semantic_copy_type(
            function->return_type
        );

    /*
     * Retorno FFI de ancho fijo: se promueve a `int`
     * (i64) en el nivel semántico de E++. El codegen
     * extiende el valor estrecho con signo o ceros
     * según corresponda.
     */
    if (semantic_type_is_ffi_integer(result)) {

        int is_unsigned =
            result->kind == TYPE_U8 ||
            result->kind == TYPE_U16 ||
            result->kind == TYPE_U32 ||
            result->kind == TYPE_U64 ||
            result->kind == TYPE_SIZE;

        node->data.call.ffi_return_unsigned =
            is_unsigned;

        semantic_destroy_type(result);

        result =
            semantic_create_basic_type(
                TYPE_INT
            );

        if (result == NULL) {

            if (temporary_function)
                semantic_function_destroy(function);

            return NULL;
        }
    }

    /*
     * `-> string owned`: el codegen debe copiar el
     * resultado C a la arena y liberar el original.
     */
    if (function->external_string_owned)
        node->data.call.ffi_string_owned = 1;

    /*
     * Llamar a una función async produce task<T>
     * (§52). Una async fn sin -> tipo produce task<null>.
     */
    if (function->is_async) {

        if (result == NULL)
            result =
                semantic_create_basic_type(
                    TYPE_NULL
                );

        if (result != NULL) {

            Type *task =
                semantic_create_basic_type(
                    TYPE_TASK
                );

            if (task == NULL) {

                semantic_destroy_type(result);

                if (temporary_function)
                    semantic_function_destroy(function);

                return NULL;
            }

            task->element_type = result;
            result = task;
        }
    }

    if (temporary_function)
        semantic_function_destroy(function);

    return result;
}


/* =========================================================
 * INDEX
 * ========================================================= */

Type *semantic_analyze_index(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *object =
        semantic_analyze_expression(
            context,
            node->data.index.object
        );

    Type *index =
        semantic_analyze_expression(
            context,
            node->data.index.index
        );

    if (object == NULL ||
        index == NULL) {

        semantic_destroy_type(object);
        semantic_destroy_type(index);

        return NULL;
    }

    Type *result = NULL;

    if (object->kind == TYPE_LIST) {

        if (index->kind != TYPE_INT) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_INDEX,
                node->line,
                node->column,
                "el indice de una lista debe ser int"
            );

        } else {

            result =
                semantic_copy_type(
                    object->element_type
                );
        }

    } else if (object->kind == TYPE_MAP) {

        if (!semantic_type_compatible(
                object->key_type,
                index
            )) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_KEY_TYPE,
                node->line,
                node->column,
                "tipo de clave invalido"
            );

        } else {

            result =
                semantic_copy_type(
                    object->value_type
                );
        }

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_INDEX,
            node->line,
            node->column,
            "el tipo no se puede indexar"
        );
    }

    semantic_destroy_type(object);
    semantic_destroy_type(index);

    return result;
}


/* =========================================================
 * MEMBER
 * ========================================================= */

Type *semantic_analyze_member(
    SemanticContext *context,
    ASTNode *node
)
{
    /*
     * Si el objeto es un alias de módulo, el miembro es
     * una función de módulo: solo se puede invocar.
     */
    if (node->data.member.object != NULL &&
        node->data.member.object->kind ==
            AST_IDENTIFIER &&
        semantic_lookup_alias(
            context,
            node->data.member.object
                ->data.identifier.name
        ) != NULL)
    {
        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "una funcion de modulo solo se puede invocar con ()"
        );

        return NULL;
    }

    Type *object =
        semantic_analyze_expression(
            context,
            node->data.member.object
        );

    if (object == NULL)
        return NULL;

    const char *member =
        node->data.member.member;

    Type *result = NULL;

    /*
     * Métodos de listas.
     */
    if (object->kind == TYPE_STRUCT) {
        SemanticSymbol *struct_symbol =
            semantic_lookup_struct(
                context,
                object->struct_name
            );

        if (struct_symbol == NULL ||
            struct_symbol->struct_decl == NULL) {
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_TYPE,
                node->line,
                node->column,
                "tipo de struct no declarado"
            );
        } else {
            ASTNodeList *fields =
                &struct_symbol->struct_decl
                    ->data.struct_decl.fields;

            for (int i = 0; i < fields->count; i++) {
                ASTNode *field = fields->items[i];

                if (field != NULL &&
                    field->data.var_decl.name != NULL &&
                    strcmp(
                        field->data.var_decl.name,
                        member
                    ) == 0) {
                    result =
                        semantic_copy_type(
                            field->data.var_decl.type
                        );
                    break;
                }
            }

            if (result == NULL) {
                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
                    node->line,
                    node->column,
                    "campo desconocido en el struct"
                );
            }
        }

    } else if (object->kind == TYPE_LIST) {

        if (strcmp(member, "len") == 0) {

            result =
                semantic_create_basic_type(
                    TYPE_INT
                );

        } else if (
            strcmp(member, "add") == 0 ||
            strcmp(member, "remove") == 0
        ) {

            /*
             * add/remove solo tienen sentido invocados:
             * numeros.add(1), no numeros.add.
             * La llamada se analiza en
             * semantic_analyze_method_call().
             */
            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_OPERATION,
                node->line,
                node->column,
                "el metodo de lista se debe invocar con ()"
            );

            result = NULL;

        } else {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
                node->line,
                node->column,
                "miembro desconocido de list"
            );
        }

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "el tipo no tiene miembros"
        );
    }

    semantic_destroy_type(object);

    return result;
}


/* =========================================================
 * LISTA
 * ========================================================= */

Type *semantic_analyze_list(
    SemanticContext *context,
    ASTNode *node
)
{
    if (node == NULL)
        return NULL;

    if (node->data.list_literal.elements.count == 0) {

        /*
         * Una lista vacía necesita inferencia de tipo
         * desde el contexto. Como este AST no guarda
         * contexto esperado, no podemos determinar T.
         */
        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
            node->line,
            node->column,
            "cannot infer the type of an empty list"
        );

        return NULL;
    }

    Type *element_type = NULL;

    for (int i = 0;
         i < node->data.list_literal.elements.count;
         i++) {

        Type *current =
            semantic_analyze_expression(
                context,
                node->data.list_literal.elements.items[i]
            );

        if (current == NULL) {

            semantic_destroy_type(element_type);
            return NULL;
        }

        if (element_type == NULL) {

            element_type = current;

        } else if (
            !semantic_types_equal(
                element_type,
                current
            )
        ) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
                node->line,
                node->column,
                "los elementos de una lista deben tener el mismo tipo"
            );

            semantic_destroy_type(element_type);
            semantic_destroy_type(current);

            return NULL;

        } else {

            semantic_destroy_type(current);
        }
    }

    return semantic_create_list_type(
        element_type
    );
}


/* =========================================================
 * MAP
 * ========================================================= */

Type *semantic_analyze_map(
    SemanticContext *context,
    ASTNode *node
)
{
    if (node == NULL)
        return NULL;

    if (node->data.map_literal.entries.count == 0) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_KEY_TYPE,
            node->line,
            node->column,
            "no se puede determinar el tipo de un map vacio"
        );

        return NULL;
    }

    Type *key_type = NULL;
    Type *value_type = NULL;

    for (int i = 0;
         i < node->data.map_literal.entries.count;
         i++) {

        ASTMapEntry *entry =
            node->data.map_literal.entries.items[i];

        Type *current_key =
            semantic_analyze_expression(
                context,
                entry->key
            );

        Type *current_value =
            semantic_analyze_expression(
                context,
                entry->value
            );

        if (current_key == NULL ||
            current_value == NULL) {

            semantic_destroy_type(current_key);
            semantic_destroy_type(current_value);

            semantic_destroy_type(key_type);
            semantic_destroy_type(value_type);

            return NULL;
        }

        if (key_type == NULL) {

            key_type = current_key;

        } else {

            if (!semantic_types_equal(
                    key_type,
                    current_key
                )) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_INVALID_KEY_TYPE,
                    node->line,
                    node->column,
                    "las claves del map deben tener el mismo tipo"
                );

                semantic_destroy_type(current_key);
                semantic_destroy_type(current_value);
                semantic_destroy_type(key_type);
                semantic_destroy_type(value_type);

                return NULL;
            }

            semantic_destroy_type(current_key);
        }

        if (value_type == NULL) {

            value_type = current_value;

        } else {

            if (!semantic_types_equal(
                    value_type,
                    current_value
                )) {

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_TYPE_MISMATCH,
                    node->line,
                    node->column,
                    "los valores del map deben tener el mismo tipo"
                );

                semantic_destroy_type(current_value);
                semantic_destroy_type(key_type);
                semantic_destroy_type(value_type);

                return NULL;
            }

            semantic_destroy_type(current_value);
        }
    }

    return semantic_create_map_type(
        key_type,
        value_type
    );
}


/* =========================================================
 * RETURN
 * ========================================================= */

int semantic_check_return(
    SemanticContext *context,
    Type *actual_type
)
{
    if (context == NULL)
        return 0;

    if (context->current_function == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_RETURN_OUTSIDE_FUNCTION,
            0,
            0,
            "return fuera de una funcion"
        );

        return 0;
    }

    Type *expected =
        context->current_function->return_type;

    if (expected == NULL) {

        if (actual_type != NULL) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_RETURN_TYPE,
                0,
                0,
                "una funcion sin tipo de retorno no puede devolver un valor"
            );

            return 0;
        }

        return 1;
    }

    if (actual_type == NULL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_RETURN_TYPE,
            0,
            0,
            "la funcion debe devolver un valor"
        );

        return 0;
    }

    if (!semantic_type_compatible(
            expected,
            actual_type
        )) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_RETURN_TYPE,
            0,
            0,
            "tipo de retorno incorrecto"
        );

        return 0;
    }

    return 1;
}


int semantic_analyze_return(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *value_type = NULL;

    if (node->data.return_stmt.value != NULL) {

        value_type =
            semantic_analyze_expression(
                context,
                node->data.return_stmt.value
            );
    }

    int result =
        semantic_check_return(
            context,
            value_type
        );

    semantic_destroy_type(value_type);

    return result;
}


/* =========================================================
 * IF
 * ========================================================= */

int semantic_analyze_if(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *condition =
        semantic_analyze_expression(
            context,
            node->data.if_stmt.condition
        );

    if (condition == NULL)
        return 0;

    int valid = 1;

    if (!semantic_is_boolean(condition)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_TYPE_MISMATCH,
            node->line,
            node->column,
            "la condicion de if debe ser bool"
        );

        valid = 0;
    }

    semantic_destroy_type(condition);

    if (!semantic_analyze_block(
            context,
            node->data.if_stmt.then_branch
        )) {

        valid = 0;
    }

    if (node->data.if_stmt.else_branch != NULL) {

        /*
         * elif convierte else_branch en otro AST_IF
         * (azúcar sintáctico), no en un AST_BLOCK.
         */
        if (node->data.if_stmt.else_branch->kind
                == AST_BLOCK) {

            if (!semantic_analyze_block(
                    context,
                    node->data.if_stmt.else_branch
                )) {

                valid = 0;
            }

        } else {

            if (!semantic_analyze_statement(
                    context,
                    node->data.if_stmt.else_branch
                )) {

                valid = 0;
            }
        }
    }

    return valid;
}


/* =========================================================
 * WHILE
 * ========================================================= */

int semantic_analyze_while(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *condition =
        semantic_analyze_expression(
            context,
            node->data.while_stmt.condition
        );

    if (condition == NULL)
        return 0;

    int valid = 1;

    if (!semantic_is_boolean(condition)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_TYPE_MISMATCH,
            node->line,
            node->column,
            "la condicion de while debe ser bool"
        );

        valid = 0;
    }

    semantic_destroy_type(condition);

    semantic_enter_loop(context);

    if (!semantic_analyze_block(
            context,
            node->data.while_stmt.body
        )) {

        valid = 0;
    }

    semantic_leave_loop(context);

    return valid;
}


/* =========================================================
 * FOR
 * ========================================================= */

int semantic_analyze_for(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *iterable =
        semantic_analyze_expression(
            context,
            node->data.for_stmt.iterable
        );

    if (iterable == NULL)
        return 0;

    int valid = 1;

    Type *element_type = NULL;

    if (iterable->kind == TYPE_LIST) {

        element_type =
            semantic_copy_type(
                iterable->element_type
            );

    } else if (iterable->kind == TYPE_MAP) {

        /*
         * Por ahora for sobre map itera sus claves.
         */
        element_type =
            semantic_copy_type(
                iterable->key_type
            );

    } else {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_ELEMENT_TYPE,
            node->line,
            node->column,
            "for necesita una list o map"
        );

        valid = 0;
    }

    semantic_destroy_type(iterable);

    if (element_type == NULL)
        return 0;

    node->data.for_stmt.element_type =
        semantic_copy_type(element_type);

    if (node->data.for_stmt.element_type == NULL) {

        semantic_destroy_type(element_type);
        return 0;
    }

    if (!semantic_enter_scope(context)) {

        semantic_destroy_type(element_type);
        return 0;
    }

    SemanticSymbol *symbol =
        calloc(
            1,
            sizeof(SemanticSymbol)
        );

    if (symbol != NULL) {

        symbol->name =
            semantic_strdup(
                node->data.for_stmt.variable
            );

        symbol->type = element_type;
        symbol->kind = SYMBOL_VARIABLE;
        symbol->is_initialized = 1;
        symbol->is_const = 0;

        if (!semantic_declare_symbol(
                context,
                symbol
            )) {

            semantic_symbol_destroy(symbol);
            valid = 0;
        }

    } else {

        semantic_destroy_type(element_type);
        valid = 0;
    }

    semantic_enter_loop(context);

    if (!semantic_analyze_block(
            context,
            node->data.for_stmt.body
        )) {

        valid = 0;
    }

    semantic_leave_loop(context);

    semantic_leave_scope(context);

    return valid;
}


/* =========================================================
 * BREAK / CONTINUE
 * ========================================================= */

void semantic_enter_loop(
    SemanticContext *context
)
{
    if (context != NULL)
        context->loop_depth++;
}


void semantic_leave_loop(
    SemanticContext *context
)
{
    if (context != NULL &&
        context->loop_depth > 0) {

        context->loop_depth--;
    }
}


int semantic_inside_loop(
    SemanticContext *context
)
{
    return context != NULL &&
           context->loop_depth > 0;
}


int semantic_inside_function(
    SemanticContext *context
)
{
    return context != NULL &&
           context->function_depth > 0;
}


int semantic_analyze_break(
    SemanticContext *context,
    ASTNode *node
)
{
    if (!semantic_inside_loop(context)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_BREAK,
            node->line,
            node->column,
            "break solo puede utilizarse dentro de un bucle"
        );

        return 0;
    }

    return 1;
}


int semantic_analyze_continue(
    SemanticContext *context,
    ASTNode *node
)
{
    if (!semantic_inside_loop(context)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_CONTINUE,
            node->line,
            node->column,
            "continue solo puede utilizarse dentro de un bucle"
        );

        return 0;
    }

    return 1;
}


/* =========================================================
 * TRY
 * ========================================================= */

int semantic_analyze_try(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    int valid = 1;

    /*
     * try (decisión de diseño 2026-10-08, claude.md §15):
     *
     * - el cuerpo del try se analiza con try_depth > 0:
     *   return/break/continue están prohibidos dentro
     *   (el manejador activo no debe cruzar el salto).
     * - cada catch crea un scope con su variable de error
     *   declarada como string inicializado; el catch se
     *   analiza ya con try_depth restaurado (su propio
     *   manejador se desapila antes de ejecutar el
     *   cuerpo, igual que en el codegen).
     */
    context->try_depth++;

    if (node->data.try_stmt.try_body != NULL) {

        if (!semantic_analyze_block(
                context,
                node->data.try_stmt.try_body
            )) {

            valid = 0;
        }
    }

    context->try_depth--;

    for (int i = 0;
         i < node->data.try_stmt.catches.count;
         i++) {

        ASTNode *catch_node =
            node->data.try_stmt.catches.items[i];

        if (catch_node == NULL)
            continue;

        if (!semantic_enter_scope(context)) {

            valid = 0;
            continue;
        }

        SemanticSymbol *symbol =
            calloc(
                1,
                sizeof(SemanticSymbol)
            );

        if (symbol == NULL) {

            semantic_leave_scope(context);
            valid = 0;
            continue;
        }

        symbol->name =
            semantic_strdup(
                catch_node->data.catch_stmt.error_name
            );

        symbol->type =
            semantic_create_basic_type(TYPE_STRING);

        symbol->kind =
            SYMBOL_VARIABLE;

        symbol->is_private = 0;
        symbol->is_const = 0;
        symbol->is_initialized = 1;

        symbol->line = catch_node->line;
        symbol->column = catch_node->column;

        if (symbol->name == NULL ||
            symbol->type == NULL ||
            !semantic_declare_symbol(
                context,
                symbol
            )) {

            semantic_symbol_destroy(symbol);
            semantic_leave_scope(context);
            valid = 0;
            continue;
        }

        if (!semantic_analyze_block(
                context,
                catch_node->data.catch_stmt.body
            )) {

            valid = 0;
        }

        semantic_leave_scope(context);
    }

    return valid;
}


/* =========================================================
 * BLOCK
 * ========================================================= */

int semantic_analyze_block(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    if (node->kind != AST_BLOCK)
        return 0;

    int valid = 1;

    /*
     * Los bloques normales crean scope.
     *
     * El body de una función ya está dentro de un scope
     * creado por semantic_analyze_function(), por lo que
     * aquí usamos otro scope únicamente para bloques
     * anidados.
     */
    int create_scope = 1;

    if (context->current_scope == NULL)
        create_scope = 0;

    if (create_scope)
        semantic_enter_scope(context);

    for (int i = 0;
         i < node->data.block.statements.count;
         i++) {

        ASTNode *statement =
            node->data.block.statements.items[i];

        if (statement == NULL)
            continue;

        if (!semantic_analyze_statement(
                context,
                statement
            )) {

            valid = 0;
        }
    }

    if (create_scope)
        semantic_leave_scope(context);

    return valid;
}


/* =========================================================
 * STATEMENTS
 * ========================================================= */

int semantic_analyze_statement(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    /*
     * return/break/continue dentro de un try activo
     * (decisión de diseño 2026-10-08, claude.md §15):
     * el salto dejaría el manejador de ese try apilado
     * apuntando a un marco que ya no debe saltar ahí.
     * En lugar de bookkeeping de unwind, se prohíben.
     *
     * Cubre también los bucles y bloques anidados dentro
     * del cuerpo del try: cualquier profundidad > 0.
     */
    if (context->try_depth > 0 &&
        (node->kind == AST_RETURN ||
         node->kind == AST_BREAK ||
         node->kind == AST_CONTINUE)) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "return, break and continue are not allowed "
            "inside try"
        );

        return 0;
    }

    switch (node->kind) {

        case AST_VAR_DECL:
            return semantic_analyze_var(
                context,
                node
            );

        case AST_CONST_DECL:
            return semantic_analyze_const(
                context,
                node
            );

        case AST_FUNCTION_DECL:
            return semantic_analyze_function(
                context,
                node
            );

        case AST_STRUCT_DECL:
            return semantic_analyze_struct(
                context,
                node
            );

        case AST_IMPORT:
            return semantic_analyze_import(
                context,
                node
            );

        case AST_FROM_IMPORT:
            return semantic_analyze_from_import(
                context,
                node
            );

        case AST_BLOCK:
            return semantic_analyze_block(
                context,
                node
            );

        case AST_EXPRESSION_STMT:
        {
            int had_error_before =
                context->had_error;

            Type *type =
                semantic_analyze_expression(
                    context,
                    node->data.expression_stmt.expression
                );

            semantic_destroy_type(type);

            /*
             * Una expresión sin valor (p. ej. print(...),
             * cuya función no devuelve nada) es una
             * sentencia válida: el fallo se detecta con los
             * errores reportados, no con el tipo retornado.
             */
            return context->had_error ==
                       had_error_before &&
                   (type != NULL ||
                    !context->had_error);
        }

        case AST_RETURN:
            return semantic_analyze_return(
                context,
                node
            );

        case AST_IF:
            return semantic_analyze_if(
                context,
                node
            );

        case AST_WHILE:
            return semantic_analyze_while(
                context,
                node
            );

        case AST_FOR:
            return semantic_analyze_for(
                context,
                node
            );

        case AST_BREAK:
            return semantic_analyze_break(
                context,
                node
            );

        case AST_CONTINUE:
            return semantic_analyze_continue(
                context,
                node
            );

        case AST_PASS:
            return 1;

        case AST_TRY:
            return semantic_analyze_try(
                context,
                node
            );

        default:
            return semantic_analyze_expression(
                context,
                node
            ) != NULL;
    }
}


/* =========================================================
 * EXPRESSIONS
 * ========================================================= */

/*
 * f-string: el resultado siempre es string.
 *
 * Cada interpolación {expr} debe ser int, float, string,
 * bool o char (decisión de diseño 2026-10-08): list,
 * map y null no se pueden convertir a texto todavía.
 */
static Type *semantic_analyze_fstring(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return NULL;

    const ASTFStringSegmentList *segments =
        &node->data.fstring.segments;

    for (int i = 0; i < segments->count; i++) {

        ASTFStringSegment *segment =
            segments->items[i];

        if (segment == NULL ||
            !segment->is_expr)
            continue;

        Type *type = semantic_analyze_expression(
            context,
            segment->expr
        );

        if (type == NULL)
            return NULL;

        switch (type->kind) {

            case TYPE_INT:
            case TYPE_FLOAT:
            case TYPE_STRING:
            case TYPE_BOOL:
            case TYPE_CHAR:
                break;

            default:
            {
                char message[256];

                snprintf(
                    message,
                    sizeof(message),
                    "solo se pueden interpolar int, "
                    "float, string, bool o char "
                    "en un f-string; se encontro %s",
                    semantic_type_name(type)
                );

                semantic_set_error(
                    context,
                    SEMANTIC_ERROR_INVALID_OPERATION,
                    node->line,
                    node->column,
                    message
                );

                semantic_destroy_type(type);
                return NULL;
            }
        }

        semantic_destroy_type(type);
    }

    return semantic_create_basic_type(
        TYPE_STRING
    );
}


static Type *semantic_check_expression(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    switch (node->kind) {

        case AST_IDENTIFIER:
            return semantic_analyze_identifier(
                context,
                node
            );


        case AST_INT_LITERAL:
            return semantic_create_basic_type(
                TYPE_INT
            );


        case AST_FLOAT_LITERAL:
            return semantic_create_basic_type(
                TYPE_FLOAT
            );


        case AST_STRING_LITERAL:
            return semantic_create_basic_type(
                TYPE_STRING
            );


        case AST_CHAR_LITERAL:
            return semantic_create_basic_type(
                TYPE_CHAR
            );


        case AST_BOOLEAN_LITERAL:
            return semantic_create_basic_type(
                TYPE_BOOL
            );


        case AST_NULL_LITERAL:
            return semantic_create_basic_type(
                TYPE_NULL
            );


        case AST_BINARY:
            return semantic_analyze_binary(
                context,
                node
            );


        case AST_UNARY:
            return semantic_analyze_unary(
                context,
                node
            );


        case AST_AWAIT:
            return semantic_analyze_await(
                context,
                node
            );


        case AST_ASSIGN:
            return semantic_analyze_assignment(
                context,
                node
            );


        case AST_CALL:
            return semantic_analyze_call(
                context,
                node
            );


        case AST_INDEX:
            return semantic_analyze_index(
                context,
                node
            );


        case AST_MEMBER:
            return semantic_analyze_member(
                context,
                node
            );


        case AST_LIST_LITERAL:
            return semantic_analyze_list(
                context,
                node
            );


        case AST_MAP_LITERAL:
            return semantic_analyze_map(
                context,
                node
            );


        case AST_FSTRING:
            return semantic_analyze_fstring(
                context,
                node
            );


        default:

            semantic_set_error(
                context,
                SEMANTIC_ERROR_INVALID_OPERATION,
                node->line,
                node->column,
                "expresion no soportada por el analizador semantico"
            );

            return NULL;
    }
}


/*
 * Análisis público de expresiones.
 *
 * Además de devolver el tipo (propiedad del llamador, que
 * lo destruye), guarda una copia en el nodo para que el
 * backend conozca el tipo estático de cada expresión:
 *
 *     numeros[0]      -> int       (list<int>)
 *     edades["Ana"]   -> int       (map<string, int>)
 *     len(numeros)    -> list<int> en el argumento
 *
 * Sin esta anotación llvm.c tendría que adivinar si un
 * puntero es una lista o un map.
 */
Type *semantic_analyze_expression(
    SemanticContext *context,
    ASTNode *node
)
{
    Type *result =
        semantic_check_expression(
            context,
            node
        );

    if (result != NULL) {

        /* Por si el mismo nodo se analiza más de una vez. */
        semantic_destroy_type(node->expr_type);

        node->expr_type =
            semantic_copy_type(result);
    }

    return result;
}


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

int semantic_analyze_declaration(
    SemanticContext *context,
    ASTNode *node
)
{
    if (context == NULL ||
        node == NULL)
        return 0;

    /*
     * Sin globales (decisión 2026-10-08, ver claude.md §4):
     * var/const solo dentro de una función. A nivel raíz el
     * codegen no tiene bloque donde colocar el alloca y
     * terminaba en SIGSEGV (src/llvm.c
     * llvm_create_variable). Los dos llamantes de esta
     * función (semantic_analyze y
     * semantic_analyze_module_declarations) recorren el
     * nivel superior; el cuerpo de las funciones pasa por
     * semantic_analyze_statement y sigue permitido.
     */
    if (node->kind == AST_VAR_DECL ||
        node->kind == AST_CONST_DECL) {

        semantic_set_error(
            context,
            SEMANTIC_ERROR_INVALID_OPERATION,
            node->line,
            node->column,
            "las variables y constantes se declaran "
            "dentro de una funcion"
        );

        return 0;
    }

    switch (node->kind) {

        case AST_VAR_DECL:
            return semantic_analyze_var(
                context,
                node
            );

        case AST_CONST_DECL:
            return semantic_analyze_const(
                context,
                node
            );

        case AST_FUNCTION_DECL:
            return semantic_analyze_function(
                context,
                node
            );

        case AST_IMPORT:
            return semantic_analyze_import(
                context,
                node
            );

        case AST_FROM_IMPORT:
            return semantic_analyze_from_import(
                context,
                node
            );

        default:
            return semantic_analyze_statement(
                context,
                node
            );
    }
}


/* =========================================================
 * PROGRAMA
 * ========================================================= */

int semantic_analyze(
    SemanticContext *context,
    ASTNode *program
)
{
    if (context == NULL ||
        program == NULL)
        return 0;

    if (program->kind != AST_PROGRAM)
        return 0;

    int valid = 1;

    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {
        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node != NULL &&
            node->kind == AST_STRUCT_DECL &&
            !semantic_register_struct(context, node))
            valid = 0;
    }

    /*
     * Primera pasada:
     * registrar funciones y símbolos globales.
     *
     * Esto permite:
     *
     * fn a():
     *     b()
     *
     * fn b():
     *     ...
     *
     * aunque b aparezca después.
     */

    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {

        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node == NULL)
            continue;

        if (node->kind == AST_IMPORT) {

            semantic_register_import(
                context,
                node
            );

            continue;
        }

        if (node->kind == AST_FROM_IMPORT) {

            semantic_register_from_import(
                context,
                node
            );

            continue;
        }

        if (node->kind != AST_FUNCTION_DECL)
            continue;

        if (semantic_lookup_current_scope(
                context,
                node->data.function_decl.name
            ) != NULL) {

            semantic_set_error(
                context,
                SEMANTIC_ERROR_DUPLICATE_NAME,
                node->line,
                node->column,
                "la funcion ya esta declarada"
            );

            valid = 0;
            continue;
        }

        SemanticFunction *function =
            semantic_function_from_ast(node);

        if (function == NULL) {

            valid = 0;
            continue;
        }

        if (!semantic_check_async_function(
                context,
                function,
                node->line,
                node->column
            )) {

            semantic_function_destroy(function);
            valid = 0;
            continue;
        }

        SemanticSymbol *symbol =
            calloc(
                1,
                sizeof(SemanticSymbol)
            );

        if (symbol == NULL) {

            semantic_function_destroy(function);
            valid = 0;
            continue;
        }

        symbol->name =
            semantic_strdup(function->name);

        symbol->type =
            semantic_copy_type(
                function->return_type
            );

        symbol->kind =
            SYMBOL_FUNCTION;

        symbol->is_private =
            function->is_private;

        symbol->is_const = 1;
        symbol->is_initialized = 1;
        symbol->function = function;

        symbol->line = node->line;
        symbol->column = node->column;

        if (!semantic_declare_symbol(
                context,
                symbol
            )) {

            semantic_symbol_destroy(symbol);
            valid = 0;
        }
    }

    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {
        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node != NULL &&
            node->kind == AST_STRUCT_DECL &&
            !semantic_analyze_struct(context, node))
            valid = 0;
    }

    /*
     * Segunda pasada:
     * analizar todo.
     */
    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {

        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node == NULL)
            continue;

        if (node->kind == AST_STRUCT_DECL)
            continue;

        /*
         * Las funciones ya fueron registradas arriba,
         * pero aquí necesitamos analizar su cuerpo.
         *
         * Evitamos volver a declararlas.
         */
        if (node->kind == AST_FUNCTION_DECL) {

            if (!semantic_analyze_registered_function(
                    context,
                    node
                )) {

                valid = 0;
            }

            continue;
        }

        /*
         * Módulos cargados por el loader: analizar sus
         * declaraciones dentro de su propio scope.
         */
        if (node->kind == AST_IMPORT ||
            node->kind == AST_FROM_IMPORT)
        {
            if (!semantic_analyze_module_declarations(
                    context,
                    node
                )) {

                valid = 0;
            }

            continue;
        }

        if (!semantic_analyze_declaration(
                context,
                node
            )) {

            valid = 0;
        }
    }

    /*
     * El espacio de nombres es plano: detectar
     * colisiones entre funciones del principal y de
     * los módulos.
     */
    if (!semantic_check_module_collisions(
            context,
            &program->data.program.declarations
        )) {

        valid = 0;
    }

    return valid && !context->had_error;
}