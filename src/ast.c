#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * CONSTANTES INTERNAS
 * ========================================================= */

#define AST_INITIAL_LIST_CAPACITY 16


/* =========================================================
 * FUNCIONES INTERNAS
 * ========================================================= */

/*
 * Destruye un Type perteneciente al AST.
 *
 * El parser crea estos Types con malloc() y el AST es
 * su propietario. El análisis semántico trabaja siempre
 * con copias (semantic_copy_type), por lo que destruirlos
 * aquí no produce doble liberación.
 */
static void ast_destroy_type(Type *type)
{
    if (type == NULL)
        return;

    ast_destroy_type(type->element_type);
    ast_destroy_type(type->key_type);
    ast_destroy_type(type->value_type);
    ast_destroy_type(type->return_type);

    if (type->parameter_types != NULL) {
        for (int i = 0; i < type->parameter_count; i++)
            ast_destroy_type(type->parameter_types[i]);
    }

    free(type->parameter_types);
    free(type->struct_name);

    free(type);
}


static int ast_node_list_grow(ASTNodeList *list)
{
    if (list == NULL)
        return 0;

    int new_capacity;

    if (list->capacity == 0)
        new_capacity = AST_INITIAL_LIST_CAPACITY;
    else
        new_capacity = list->capacity * 2;

    ASTNode **new_items = realloc(
        list->items,
        sizeof(ASTNode *) * new_capacity
    );

    if (new_items == NULL)
        return 0;

    list->items = new_items;
    list->capacity = new_capacity;

    return 1;
}


static int ast_parameter_list_grow(ASTParameterList *list)
{
    if (list == NULL)
        return 0;

    int new_capacity;

    if (list->capacity == 0)
        new_capacity = AST_INITIAL_LIST_CAPACITY;
    else
        new_capacity = list->capacity * 2;

    ASTParameter **new_items = realloc(
        list->items,
        sizeof(ASTParameter *) * new_capacity
    );

    if (new_items == NULL)
        return 0;

    list->items = new_items;
    list->capacity = new_capacity;

    return 1;
}


static int ast_map_entry_list_grow(ASTMapEntryList *list)
{
    if (list == NULL)
        return 0;

    int new_capacity;

    if (list->capacity == 0)
        new_capacity = AST_INITIAL_LIST_CAPACITY;
    else
        new_capacity = list->capacity * 2;

    ASTMapEntry **new_items = realloc(
        list->items,
        sizeof(ASTMapEntry *) * new_capacity
    );

    if (new_items == NULL)
        return 0;

    list->items = new_items;
    list->capacity = new_capacity;

    return 1;
}


static int ast_fstring_segment_list_grow(
    ASTFStringSegmentList *list
)
{
    if (list == NULL)
        return 0;

    int new_capacity;

    if (list->capacity == 0)
        new_capacity = AST_INITIAL_LIST_CAPACITY;
    else
        new_capacity = list->capacity * 2;

    ASTFStringSegment **new_items = realloc(
        list->items,
        sizeof(ASTFStringSegment *) * new_capacity
    );

    if (new_items == NULL)
        return 0;

    list->items = new_items;
    list->capacity = new_capacity;

    return 1;
}


/* =========================================================
 * CREACIÓN
 * ========================================================= */

ASTNode *ast_create(
    ASTNodeKind kind
)
{
    ASTNode *node = malloc(sizeof(ASTNode));

    if (node == NULL)
        return NULL;

    memset(node, 0, sizeof(ASTNode));

    node->kind = kind;

    node->line = 0;
    node->column = 0;

    return node;
}


/* =========================================================
 * DESTRUCCIÓN
 * ========================================================= */

void ast_destroy(
    ASTNode *node
)
{
    if (node == NULL)
        return;

    /*
     * Tipo de expresión rellenado por la semántica.
     * Se libera antes del switch: el resto del nodo no
     * depende de él.
     */
    ast_destroy_type(node->expr_type);
    node->expr_type = NULL;

    switch (node->kind) {

        /* -------------------------------------------------
         * PROGRAMA
         * ------------------------------------------------- */

        case AST_PROGRAM:
            ast_node_list_destroy(
                &node->data.program.declarations
            );
            break;


        /* -------------------------------------------------
         * DECLARACIONES
         * ------------------------------------------------- */

        case AST_VAR_DECL:
            free(node->data.var_decl.name);
            ast_destroy_type(node->data.var_decl.type);
            ast_destroy(node->data.var_decl.value);
            break;

        case AST_CONST_DECL:
            free(node->data.const_decl.name);
            ast_destroy_type(node->data.const_decl.type);
            ast_destroy(node->data.const_decl.value);
            break;

        case AST_FUNCTION_DECL:
            free(node->data.function_decl.name);
            ast_parameter_list_destroy(
                &node->data.function_decl.parameters
            );
            ast_destroy_type(
                node->data.function_decl.return_type
            );
            free(node->data.function_decl.external_path);
            ast_destroy(node->data.function_decl.body);
            break;

        case AST_STRUCT_DECL:
            free(node->data.struct_decl.name);
            ast_node_list_destroy(
                &node->data.struct_decl.fields
            );
            break;


        /* -------------------------------------------------
         * IMPORTS
         * ------------------------------------------------- */

        case AST_IMPORT:
            free(node->data.import_decl.module);
            free(node->data.import_decl.alias);
            ast_node_list_destroy(
                &node->data.import_decl.declarations
            );
            break;

        case AST_FROM_IMPORT:
            free(node->data.from_import.module);
            ast_node_list_destroy(
                &node->data.from_import.names
            );
            ast_node_list_destroy(
                &node->data.from_import.declarations
            );
            break;


        /* -------------------------------------------------
         * BLOQUES Y SENTENCIAS
         * ------------------------------------------------- */

        case AST_BLOCK:
            ast_node_list_destroy(
                &node->data.block.statements
            );
            break;

        case AST_EXPRESSION_STMT:
            ast_destroy(
                node->data.expression_stmt.expression
            );
            break;

        case AST_RETURN:
            ast_destroy(node->data.return_stmt.value);
            break;

        case AST_IF:
            ast_destroy(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_branch);
            ast_destroy(node->data.if_stmt.else_branch);
            break;

        case AST_WHILE:
            ast_destroy(node->data.while_stmt.condition);
            ast_destroy(node->data.while_stmt.body);
            break;

        case AST_FOR:
            free(node->data.for_stmt.variable);
            ast_destroy_type(node->data.for_stmt.element_type);
            ast_destroy(node->data.for_stmt.iterable);
            ast_destroy(node->data.for_stmt.body);
            break;

        case AST_BREAK:
        case AST_CONTINUE:
        case AST_PASS:
            break;


        /* -------------------------------------------------
         * TRY / CATCH
         * ------------------------------------------------- */

        case AST_TRY:
            ast_destroy(node->data.try_stmt.try_body);
            ast_node_list_destroy(
                &node->data.try_stmt.catches
            );
            break;

        case AST_CATCH:
            free(node->data.catch_stmt.error_name);
            ast_destroy_type(node->data.catch_stmt.error_type);
            ast_destroy(node->data.catch_stmt.body);
            break;


        /* -------------------------------------------------
         * EXPRESIONES
         * ------------------------------------------------- */

        case AST_BINARY:
            ast_destroy(node->data.binary.left);
            ast_destroy(node->data.binary.right);
            break;

        case AST_UNARY:
            ast_destroy(node->data.unary.operand);
            break;

        case AST_AWAIT:
            ast_destroy(node->data.await_expr.operand);
            break;

        case AST_ASSIGN:
            ast_destroy(node->data.assignment.target);
            ast_destroy(node->data.assignment.value);
            break;

        case AST_CALL:
            ast_destroy(node->data.call.callee);
            ast_node_list_destroy(
                &node->data.call.arguments
            );
            break;

        case AST_INDEX:
            ast_destroy(node->data.index.object);
            ast_destroy(node->data.index.index);
            break;

        case AST_MEMBER:
            ast_destroy(node->data.member.object);
            free(node->data.member.member);
            break;


        /* -------------------------------------------------
         * IDENTIFICADOR
         * ------------------------------------------------- */

        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;


        /* -------------------------------------------------
         * LITERALES
         * ------------------------------------------------- */

        case AST_INT_LITERAL:
        case AST_FLOAT_LITERAL:
        case AST_CHAR_LITERAL:
        case AST_BOOLEAN_LITERAL:
        case AST_NULL_LITERAL:
            break;

        case AST_STRING_LITERAL:
            free(node->data.string_literal.value);
            break;


        /* -------------------------------------------------
         * COLECCIONES
         * ------------------------------------------------- */

        case AST_LIST_LITERAL:
            ast_node_list_destroy(
                &node->data.list_literal.elements
            );
            break;

        case AST_MAP_LITERAL:
            ast_map_entry_list_destroy(
                &node->data.map_literal.entries
            );
            break;

        case AST_FSTRING:
            ast_fstring_segment_list_destroy(
                &node->data.fstring.segments
            );
            break;

        default:
            break;
    }

    free(node);
}


/* =========================================================
 * LISTAS DE NODOS
 * ========================================================= */

void ast_node_list_init(
    ASTNodeList *list
)
{
    if (list == NULL)
        return;

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}


void ast_node_list_add(
    ASTNodeList *list,
    ASTNode *node
)
{
    if (list == NULL || node == NULL)
        return;

    if (list->count >= list->capacity) {

        /*
         * Si no hay memoria suficiente el nodo no se
         * añade y su propiedad sigue siendo del llamador.
         */
        if (!ast_node_list_grow(list))
            return;
    }

    list->items[list->count] = node;
    list->count++;
}


void ast_node_list_destroy(
    ASTNodeList *list
)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++) {

        ast_destroy(list->items[i]);

        list->items[i] = NULL;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}


/* =========================================================
 * PARÁMETROS
 * ========================================================= */

ASTParameter *ast_parameter_create(
    char *name,
    Type *type
)
{
    ASTParameter *parameter =
        malloc(sizeof(ASTParameter));

    if (parameter == NULL)
        return NULL;

    /*
     * El nombre es propiedad del parámetro.
     */
    parameter->name = name;
    parameter->type = type;

    parameter->is_private =
        name != NULL && name[0] == '_';

    return parameter;
}


void ast_parameter_list_add(
    ASTParameterList *list,
    ASTParameter *parameter
)
{
    if (list == NULL || parameter == NULL)
        return;

    if (list->count >= list->capacity) {

        if (!ast_parameter_list_grow(list))
            return;
    }

    list->items[list->count] = parameter;
    list->count++;
}


void ast_parameter_list_destroy(
    ASTParameterList *list
)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++) {

        ASTParameter *parameter = list->items[i];

        if (parameter == NULL)
            continue;

        free(parameter->name);
        ast_destroy_type(parameter->type);

        free(parameter);

        list->items[i] = NULL;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}


/* =========================================================
 * ENTRADAS DE MAP
 * ========================================================= */

ASTMapEntry *ast_map_entry_create(
    ASTNode *key,
    ASTNode *value
)
{
    ASTMapEntry *entry =
        malloc(sizeof(ASTMapEntry));

    if (entry == NULL)
        return NULL;

    entry->key = key;
    entry->value = value;

    return entry;
}


void ast_map_entry_list_add(
    ASTMapEntryList *list,
    ASTMapEntry *entry
)
{
    if (list == NULL || entry == NULL)
        return;

    if (list->count >= list->capacity) {

        if (!ast_map_entry_list_grow(list))
            return;
    }

    list->items[list->count] = entry;
    list->count++;
}


void ast_map_entry_list_destroy(
    ASTMapEntryList *list
)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++) {

        ASTMapEntry *entry = list->items[i];

        if (entry == NULL)
            continue;

        ast_destroy(entry->key);
        ast_destroy(entry->value);

        free(entry);

        list->items[i] = NULL;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}


/* =========================================================
 * SEGMENTOS DE F-STRING
 * ========================================================= */

ASTFStringSegment *ast_fstring_segment_create(
    int is_expr,
    char *text,
    ASTNode *expr
)
{
    ASTFStringSegment *segment =
        malloc(sizeof(ASTFStringSegment));

    if (segment == NULL)
        return NULL;

    segment->is_expr = is_expr;
    segment->text = text;
    segment->expr = expr;

    return segment;
}


void ast_fstring_segment_list_add(
    ASTFStringSegmentList *list,
    ASTFStringSegment *segment
)
{
    if (list == NULL || segment == NULL)
        return;

    if (list->count >= list->capacity) {

        if (!ast_fstring_segment_list_grow(list))
            return;
    }

    list->items[list->count] = segment;
    list->count++;
}


void ast_fstring_segment_list_destroy(
    ASTFStringSegmentList *list
)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++) {

        ASTFStringSegment *segment =
            list->items[i];

        if (segment == NULL)
            continue;

        free(segment->text);

        ast_destroy(segment->expr);

        free(segment);

        list->items[i] = NULL;
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}


/* =========================================================
 * DEBUG
 * ========================================================= */

const char *ast_node_kind_name(
    ASTNodeKind kind
)
{
    switch (kind) {

        case AST_PROGRAM:
            return "PROGRAM";

        case AST_VAR_DECL:
            return "VAR_DECL";

        case AST_CONST_DECL:
            return "CONST_DECL";

        case AST_FUNCTION_DECL:
            return "FUNCTION_DECL";

        case AST_STRUCT_DECL:
            return "STRUCT_DECL";

        case AST_IMPORT:
            return "IMPORT";

        case AST_FROM_IMPORT:
            return "FROM_IMPORT";

        case AST_BLOCK:
            return "BLOCK";

        case AST_EXPRESSION_STMT:
            return "EXPRESSION_STMT";

        case AST_RETURN:
            return "RETURN";

        case AST_IF:
            return "IF";

        case AST_WHILE:
            return "WHILE";

        case AST_FOR:
            return "FOR";

        case AST_BREAK:
            return "BREAK";

        case AST_CONTINUE:
            return "CONTINUE";

        case AST_PASS:
            return "PASS";

        case AST_TRY:
            return "TRY";

        case AST_CATCH:
            return "CATCH";

        case AST_BINARY:
            return "BINARY";

        case AST_UNARY:
            return "UNARY";

        case AST_AWAIT:
            return "AWAIT";

        case AST_ASSIGN:
            return "ASSIGN";

        case AST_CALL:
            return "CALL";

        case AST_INDEX:
            return "INDEX";

        case AST_MEMBER:
            return "MEMBER";

        case AST_IDENTIFIER:
            return "IDENTIFIER";

        case AST_INT_LITERAL:
            return "INT_LITERAL";

        case AST_FLOAT_LITERAL:
            return "FLOAT_LITERAL";

        case AST_STRING_LITERAL:
            return "STRING_LITERAL";

        case AST_CHAR_LITERAL:
            return "CHAR_LITERAL";

        case AST_BOOLEAN_LITERAL:
            return "BOOLEAN_LITERAL";

        case AST_NULL_LITERAL:
            return "NULL_LITERAL";

        case AST_LIST_LITERAL:
            return "LIST_LITERAL";

        case AST_MAP_LITERAL:
            return "MAP_LITERAL";

        case AST_FSTRING:
            return "FSTRING";

        default:
            return "UNKNOWN";
    }
}


const char *ast_operator_name(
    ASTOperator operator
)
{
    switch (operator) {

        case AST_OP_ADD:
            return "+";

        case AST_OP_SUB:
            return "-";

        case AST_OP_MUL:
            return "*";

        case AST_OP_DIV:
            return "/";

        case AST_OP_FLOOR_DIV:
            return "//";

        case AST_OP_MOD:
            return "%";

        case AST_OP_EQUAL:
            return "==";

        case AST_OP_NOT_EQUAL:
            return "!=";

        case AST_OP_GREATER:
            return ">";

        case AST_OP_LESS:
            return "<";

        case AST_OP_GREATER_EQUAL:
            return ">=";

        case AST_OP_LESS_EQUAL:
            return "<=";

        case AST_OP_AND:
            return "and";

        case AST_OP_OR:
            return "or";

        case AST_OP_ASSIGN:
            return "=";

        case AST_OP_ADD_ASSIGN:
            return "+=";

        case AST_OP_SUB_ASSIGN:
            return "-=";

        case AST_OP_MUL_ASSIGN:
            return "*=";

        case AST_OP_DIV_ASSIGN:
            return "/=";

        case AST_OP_NEGATE:
            return "-";

        case AST_OP_NOT:
            return "not";

        case AST_OP_INCREMENT:
            return "++";

        case AST_OP_DECREMENT:
            return "--";

        default:
            return "?";
    }
}


/* =========================================================
 * IMPRESIÓN
 * ========================================================= */

static void ast_print_indent(int depth)
{
    for (int i = 0; i < depth; i++)
        printf("  ");
}


/*
 * Imprime un Type para debug.
 *
 * types.h no declara ninguna función de conversión,
 * por eso este helper vive en ast.c.
 */
static void ast_print_type(const Type *type)
{
    if (type == NULL) {
        printf("?");
        return;
    }

    switch (type->kind) {

        case TYPE_INT:
            printf("int");
            break;

        case TYPE_FLOAT:
            printf("float");
            break;

        case TYPE_STRING:
            printf("string");
            break;

        case TYPE_BOOL:
            printf("bool");
            break;

        case TYPE_CHAR:
            printf("char");
            break;

        case TYPE_NULL:
            printf("null");
            break;

        case TYPE_LIST:
            printf("list<");
            ast_print_type(type->element_type);
            printf(">");
            break;

        case TYPE_TASK:
            printf("task<");
            ast_print_type(type->element_type);
            printf(">");
            break;

        case TYPE_MAP:
            printf("map<");
            ast_print_type(type->key_type);
            printf(", ");
            ast_print_type(type->value_type);
            printf(">");
            break;

        case TYPE_STRUCT:
            printf("%s",
                type->struct_name != NULL
                    ? type->struct_name
                    : "?");
            break;

        default:
            printf("?");
            break;
    }
}


static void ast_print_node(const ASTNode *node, int depth);


static void ast_print_child(
    const ASTNode *child,
    int depth
)
{
    if (child == NULL) {
        ast_print_indent(depth);
        printf("(null)\n");
        return;
    }

    ast_print_node(child, depth);
}


static void ast_print_node(const ASTNode *node, int depth)
{
    if (node == NULL)
        return;

    ast_print_indent(depth);

    printf(
        "%s",
        ast_node_kind_name(node->kind)
    );

    switch (node->kind) {

        case AST_VAR_DECL:
            printf(" %s : ",
                node->data.var_decl.name != NULL
                    ? node->data.var_decl.name
                    : "?");
            ast_print_type(node->data.var_decl.type);
            break;

        case AST_CONST_DECL:
            printf(" %s : ",
                node->data.const_decl.name != NULL
                    ? node->data.const_decl.name
                    : "?");
            ast_print_type(node->data.const_decl.type);
            break;

        case AST_FUNCTION_DECL: {
            printf(" %s%s(",
                node->data.function_decl.is_async
                    ? "async "
                    : "",
                node->data.function_decl.name != NULL
                    ? node->data.function_decl.name
                    : "?");

            const ASTParameterList *parameters =
                &node->data.function_decl.parameters;

            for (int i = 0; i < parameters->count; i++) {

                const ASTParameter *parameter =
                    parameters->items[i];

                if (parameter == NULL)
                    continue;

                if (i > 0)
                    printf(", ");

                printf(
                    "%s : ",
                    parameter->name != NULL
                        ? parameter->name
                        : "?"
                );

                ast_print_type(parameter->type);
            }

            printf(")");

            if (node->data.function_decl.return_type != NULL) {

                printf(" -> ");
                ast_print_type(
                    node->data.function_decl.return_type
                );
            }
            break;
        }

        case AST_STRUCT_DECL: {
            printf(" %s",
                node->data.struct_decl.name != NULL
                    ? node->data.struct_decl.name
                    : "?");

            const ASTNodeList *fields =
                &node->data.struct_decl.fields;

            for (int i = 0; i < fields->count; i++) {

                const ASTNode *field = fields->items[i];

                if (field == NULL)
                    continue;

                if (i > 0)
                    printf(", ");

                printf("%s : ",
                    field->data.var_decl.name != NULL
                        ? field->data.var_decl.name
                        : "?");

                ast_print_type(field->data.var_decl.type);
            }
            break;
        }

        case AST_IMPORT:
            printf(" %s",
                node->data.import_decl.module != NULL
                    ? node->data.import_decl.module
                    : "?");

            if (node->data.import_decl.alias != NULL)
                printf(" as %s", node->data.import_decl.alias);
            break;

        case AST_FROM_IMPORT:
            printf(" %s",
                node->data.from_import.module != NULL
                    ? node->data.from_import.module
                    : "?");
            break;

        case AST_FOR:
            printf(" %s",
                node->data.for_stmt.variable != NULL
                    ? node->data.for_stmt.variable
                    : "?");
            break;

        case AST_CATCH:
            printf(" %s",
                node->data.catch_stmt.error_name != NULL
                    ? node->data.catch_stmt.error_name
                    : "?");

            if (node->data.catch_stmt.error_type != NULL) {

                printf(" : ");
                ast_print_type(
                    node->data.catch_stmt.error_type
                );
            }
            break;

        case AST_BINARY:
            printf(" %s",
                ast_operator_name(
                    node->data.binary.operator
                ));
            break;

        case AST_UNARY:
            printf(" %s",
                ast_operator_name(
                    node->data.unary.operator
                ));
            break;

        case AST_AWAIT:
            printf(" await");
            break;

        case AST_ASSIGN:
            printf(" %s",
                ast_operator_name(
                    node->data.assignment.operator
                ));
            break;

        case AST_MEMBER:
            printf(" %s",
                node->data.member.member != NULL
                    ? node->data.member.member
                    : "?");
            break;

        case AST_IDENTIFIER:
            printf(" %s",
                node->data.identifier.name != NULL
                    ? node->data.identifier.name
                    : "?");
            break;

        case AST_INT_LITERAL:
            printf(" %lld",
                node->data.int_literal.value);
            break;

        case AST_FLOAT_LITERAL:
            printf(" %g",
                node->data.float_literal.value);
            break;

        case AST_STRING_LITERAL:
            printf(" \"%s\"",
                node->data.string_literal.value != NULL
                    ? node->data.string_literal.value
                    : "");
            break;

        case AST_CHAR_LITERAL:
            printf(" '%c'",
                node->data.char_literal.value);
            break;

        case AST_BOOLEAN_LITERAL:
            printf(" %s",
                node->data.boolean_literal.value
                    ? "true"
                    : "false");
            break;

        case AST_NULL_LITERAL:
            printf(" null");
            break;

        case AST_FSTRING:
            printf(" fstring (%d segmentos)",
                node->data.fstring.segments.count);
            break;

        default:
            break;
    }

    if (node->line > 0)
        printf(" @%d:%d", node->line, node->column);

    printf("\n");


    switch (node->kind) {

        case AST_PROGRAM: {
            const ASTNodeList *list =
                &node->data.program.declarations;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_VAR_DECL:
            ast_print_child(
                node->data.var_decl.value,
                depth + 1
            );
            break;

        case AST_CONST_DECL:
            ast_print_child(
                node->data.const_decl.value,
                depth + 1
            );
            break;

        case AST_FUNCTION_DECL:
            ast_print_child(
                node->data.function_decl.body,
                depth + 1
            );
            break;

        case AST_STRUCT_DECL: {
            const ASTNodeList *fields =
                &node->data.struct_decl.fields;

            for (int i = 0; i < fields->count; i++)
                ast_print_child(fields->items[i], depth + 1);
            break;
        }

        case AST_FROM_IMPORT: {
            const ASTNodeList *list =
                &node->data.from_import.names;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_BLOCK: {
            const ASTNodeList *list =
                &node->data.block.statements;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_EXPRESSION_STMT:
            ast_print_child(
                node->data.expression_stmt.expression,
                depth + 1
            );
            break;

        case AST_RETURN:
            ast_print_child(
                node->data.return_stmt.value,
                depth + 1
            );
            break;

        case AST_IF:
            ast_print_child(
                node->data.if_stmt.condition,
                depth + 1
            );
            ast_print_child(
                node->data.if_stmt.then_branch,
                depth + 1
            );
            ast_print_child(
                node->data.if_stmt.else_branch,
                depth + 1
            );
            break;

        case AST_WHILE:
            ast_print_child(
                node->data.while_stmt.condition,
                depth + 1
            );
            ast_print_child(
                node->data.while_stmt.body,
                depth + 1
            );
            break;

        case AST_FOR:
            ast_print_child(
                node->data.for_stmt.iterable,
                depth + 1
            );
            ast_print_child(
                node->data.for_stmt.body,
                depth + 1
            );
            break;

        case AST_TRY: {
            ast_print_child(
                node->data.try_stmt.try_body,
                depth + 1
            );

            const ASTNodeList *list =
                &node->data.try_stmt.catches;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_CATCH:
            ast_print_child(
                node->data.catch_stmt.body,
                depth + 1
            );
            break;

        case AST_BINARY:
            ast_print_child(
                node->data.binary.left,
                depth + 1
            );
            ast_print_child(
                node->data.binary.right,
                depth + 1
            );
            break;

        case AST_UNARY:
            ast_print_child(
                node->data.unary.operand,
                depth + 1
            );
            break;

        case AST_ASSIGN:
            ast_print_child(
                node->data.assignment.target,
                depth + 1
            );
            ast_print_child(
                node->data.assignment.value,
                depth + 1
            );
            break;

        case AST_CALL: {
            ast_print_child(
                node->data.call.callee,
                depth + 1
            );

            const ASTNodeList *list =
                &node->data.call.arguments;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_INDEX:
            ast_print_child(
                node->data.index.object,
                depth + 1
            );
            ast_print_child(
                node->data.index.index,
                depth + 1
            );
            break;

        case AST_MEMBER:
            ast_print_child(
                node->data.member.object,
                depth + 1
            );
            break;

        case AST_LIST_LITERAL: {
            const ASTNodeList *list =
                &node->data.list_literal.elements;

            for (int i = 0; i < list->count; i++)
                ast_print_child(list->items[i], depth + 1);
            break;
        }

        case AST_MAP_LITERAL: {
            const ASTMapEntryList *list =
                &node->data.map_literal.entries;

            for (int i = 0; i < list->count; i++) {

                const ASTMapEntry *entry = list->items[i];

                if (entry == NULL)
                    continue;

                ast_print_indent(depth + 1);
                printf("MAP_ENTRY\n");

                ast_print_child(entry->key, depth + 2);
                ast_print_child(entry->value, depth + 2);
            }
            break;
        }

        case AST_FSTRING: {
            const ASTFStringSegmentList *list =
                &node->data.fstring.segments;

            for (int i = 0; i < list->count; i++) {

                const ASTFStringSegment *segment =
                    list->items[i];

                if (segment == NULL)
                    continue;

                if (segment->is_expr) {

                    ast_print_child(
                        segment->expr,
                        depth + 1
                    );

                } else {

                    ast_print_indent(depth + 1);
                    printf("TEXT \"%s\"\n",
                        segment->text != NULL
                            ? segment->text
                            : "");
                }
            }
            break;
        }

        default:
            break;
    }
}


void ast_print(
    const ASTNode *node
)
{
    if (node == NULL) {
        printf("(null)\n");
        return;
    }

    ast_print_node(node, 0);
}
