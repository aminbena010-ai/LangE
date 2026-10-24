#include "parser.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * UTILIDADES INTERNAS
 * ========================================================= */

static char *parser_strdup(const char *text)
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


/*
 * Valor de un literal de texto: el lexeme sin las comillas
 * y con los escapes resueltos (\n, \r, \t, \\, \", \', \0).
 *
 * Es la misma lista de escapes que ya se resolvían en los
 * literales de carácter: el analizador léxico la valida, así
 * que aquí solo se traduce. Antes el texto se copiaba tal
 * cual y "\n" llegaba al programa como dos caracteres.
 */
static char *parser_string_literal_value(
    const char *lexeme
)
{
    if (lexeme == NULL)
        return NULL;

    size_t length = strlen(lexeme);

    if (length < 2)
        return parser_strdup("");

    /*
     * El resultado nunca ocupa más que el interior del
     * literal (los escapes se acortan).
     */
    char *value = malloc(length - 1);

    if (value == NULL)
        return NULL;

    size_t written = 0;

    for (size_t i = 1; i + 1 < length; i++) {

        char c = lexeme[i];

        if (c == '\\' && i + 2 <= length - 1) {

            char escaped = lexeme[i + 1];

            i++;

            switch (escaped) {

                case 'n':
                    c = '\n';
                    break;

                case 'r':
                    c = '\r';
                    break;

                case 't':
                    c = '\t';
                    break;

                case '0':
                    c = '\0';
                    break;

                case '\\':
                    c = '\\';
                    break;

                case '"':
                    c = '"';
                    break;

                case '\'':
                    c = '\'';
                    break;

                default:
                    value[written++] = '\\';
                    c = escaped;
                    break;
            }
        }

        value[written++] = c;
    }

    value[written] = '\0';

    return value;
}


/*
 * Valor de un trozo de texto de un f-string: escapes
 * (\n, \r, \t, \0, \\, \", \', \0) resueltos igual que en
 * un string normal, más los trozos de llave {{ y }} que
 * el lexer dejó en crudo.
 *
 * Los delimitadores { } que abren/cierran interpolación
 * nunca llegan aquí: el lexer los emite como tokens.
 */
static char *parser_fstring_text_value(
    const char *text
)
{
    if (text == NULL)
        return NULL;

    size_t length = strlen(text);

    char *value = malloc(length + 1);

    if (value == NULL)
        return NULL;

    size_t written = 0;

    for (size_t i = 0; i < length; i++) {

        char c = text[i];

        if (c == '\\' && i + 1 < length) {

            char escaped = text[i + 1];

            i++;

            switch (escaped) {

                case 'n':
                    c = '\n';
                    break;

                case 'r':
                    c = '\r';
                    break;

                case 't':
                    c = '\t';
                    break;

                case '0':
                    c = '\0';
                    break;

                case '\\':
                    c = '\\';
                    break;

                case '"':
                    c = '"';
                    break;

                case '\'':
                    c = '\'';
                    break;

                default:
                    value[written++] = '\\';
                    c = escaped;
                    break;
            }

        } else if (c == '{' &&
                   i + 1 < length &&
                   text[i + 1] == '{') {

            c = '{';
            i++;

        } else if (c == '}' &&
                   i + 1 < length &&
                   text[i + 1] == '}') {

            c = '}';
            i++;
        }

        value[written++] = c;
    }

    value[written] = '\0';

    return value;
}


static void parser_error(
    Parser *parser,
    const char *message
)
{
    if (parser == NULL)
        return;

    /*
     * Un error de lexer ya reportado invalida el archivo;
     * lo que venga despues es en cascada y solo confunde.
     */
    if (parser->lex_error)
        return;

    parser->had_error = 1;

    error_render(
        "SyntaxError",
        ERROR_ERROR,
        parser->lexer != NULL &&
        parser->lexer->filename != NULL
            ? parser->lexer->filename
            : "?",
        parser->lexer != NULL
            ? parser->lexer->source
            : NULL,
        parser->current.line,
        parser->current.column,
        parser->current.length,
        message,
        NULL,
        NULL
    );
}


static void parser_error_at(
    Parser *parser,
    const char *message,
    Token *token
)
{
    if (parser == NULL || token == NULL)
        return;

    if (parser->lex_error)
        return;

    parser->had_error = 1;

    error_render(
        "SyntaxError",
        ERROR_ERROR,
        parser->lexer != NULL &&
        parser->lexer->filename != NULL
            ? parser->lexer->filename
            : "?",
        parser->lexer != NULL
            ? parser->lexer->source
            : NULL,
        token->line,
        token->column,
        token->length,
        message,
        NULL,
        NULL
    );
}


static void parser_destroy_token(Token *token)
{
    if (token == NULL)
        return;

    token_destroy(token);
}


static ASTNode *parser_create_node(
    Parser *parser,
    ASTNodeKind kind
)
{
    ASTNode *node = ast_create(kind);

    if (node == NULL) {

        parser_error(
            parser,
            "no se pudo reservar memoria para el AST"
        );

        return NULL;
    }

    if (parser != NULL) {
        node->line = parser->current.line;
        node->column = parser->current.column;
    }

    return node;
}


/* =========================================================
 * CONSTRUCTOR / DESTRUCTOR
 * ========================================================= */

/*
 * Un token de error con lexeme vacio significa que el lexer
 * ya imprimio el diagnostico (p. ej. E1011: comentario de
 * varias lineas sin cerrar). Solo se marca had_error y se
 * salta el token para no repetir el mensaje.
 */
static void parser_skip_reported_lex_error(
    Parser *parser
)
{
    while (parser != NULL &&
           parser->current.type == TOKEN_ERROR &&
           parser->current.length == 0) {

        parser->had_error = 1;
        parser->lex_error = 1;

        parser_destroy_token(&parser->current);

        parser->current = parser->peek;

        parser->peek = lexer_next_token(
            parser->lexer
        );
    }
}


Parser *parser_create(Lexer *lexer)
{
    if (lexer == NULL)
        return NULL;

    Parser *parser = malloc(sizeof(Parser));

    if (parser == NULL)
        return NULL;

    parser->lexer = lexer;
    parser->had_error = 0;
    parser->lex_error = 0;

    parser->current.type = TOKEN_EOF;
    parser->current.lexeme = NULL;
    parser->current.line = 0;
    parser->current.column = 0;
    parser->current.length = 0;

    parser->peek.type = TOKEN_EOF;
    parser->peek.lexeme = NULL;
    parser->peek.line = 0;
    parser->peek.column = 0;
    parser->peek.length = 0;

    parser->current = lexer_next_token(lexer);
    parser->peek = lexer_next_token(lexer);

    parser_skip_reported_lex_error(parser);

    return parser;
}


void parser_destroy(Parser *parser)
{
    if (parser == NULL)
        return;

    parser_destroy_token(&parser->current);
    parser_destroy_token(&parser->peek);

    free(parser);
}


/* =========================================================
 * TOKENS
 * ========================================================= */

void parser_advance(Parser *parser)
{
    if (parser == NULL)
        return;

    parser_destroy_token(&parser->current);

    parser->current = parser->peek;

    parser->peek = lexer_next_token(
        parser->lexer
    );

    parser_skip_reported_lex_error(parser);
}


int parser_check(
    Parser *parser,
    TokenType type
)
{
    if (parser == NULL)
        return 0;

    return parser->current.type == type;
}


int parser_expect(
    Parser *parser,
    TokenType type
)
{
    if (parser == NULL)
        return 0;

    if (parser->current.type == type) {
        parser_advance(parser);
        return 1;
    }

    char message[256];

    snprintf(
        message,
        sizeof(message),
        "se esperaba %s pero se encontro %s",
        token_type_name(type),
        token_type_name(parser->current.type)
    );

    parser_error(
        parser,
        message
    );

    return 0;
}


int parser_has_error(
    Parser *parser
)
{
    if (parser == NULL)
        return 1;

    return parser->had_error;
}


/* =========================================================
 * SINCRONIZACIÓN
 * ========================================================= */

void parser_synchronize(Parser *parser)
{
    if (parser == NULL)
        return;

    while (
        parser->current.type != TOKEN_EOF &&
        parser->current.type != TOKEN_NEWLINE &&
        parser->current.type != TOKEN_VAR &&
        parser->current.type != TOKEN_CONST &&
        parser->current.type != TOKEN_FN &&
        parser->current.type != TOKEN_ASYNC &&
        parser->current.type != TOKEN_IF &&
        parser->current.type != TOKEN_WHILE &&
        parser->current.type != TOKEN_FOR &&
        parser->current.type != TOKEN_RETURN &&
        parser->current.type != TOKEN_IMPORT &&
        parser->current.type != TOKEN_FROM &&
        parser->current.type != TOKEN_EXTERNAL
    ) {
        parser_advance(parser);
    }

    if (parser->current.type == TOKEN_NEWLINE)
        parser_advance(parser);
}


/* =========================================================
 * PROGRAMA
 * ========================================================= */

ASTNode *parser_parse(Parser *parser)
{
    if (parser == NULL)
        return NULL;

    ASTNode *program =
        parser_create_node(
            parser,
            AST_PROGRAM
        );

    if (program == NULL)
        return NULL;

    while (
        !parser_check(parser, TOKEN_EOF)
    ) {

        if (parser_check(
                parser,
                TOKEN_NEWLINE
            )) {

            parser_advance(parser);
            continue;
        }

        if (parser_check(
                parser,
                TOKEN_DEDENT
            )) {

            parser_advance(parser);
            continue;
        }

        ASTNode *node =
            parser_parse_declaration(
                parser
            );

        if (node != NULL) {

            ast_node_list_add(
                &program->data.program.declarations,
                node
            );

        } else {

            if (!parser->had_error)
                parser_error(
                    parser,
                    "declaracion o sentencia invalida"
                );

            parser_synchronize(parser);
        }
    }

    return program;
}


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

ASTNode *parser_parse_declaration(
    Parser *parser
)
{
    if (parser == NULL)
        return NULL;

    switch (parser->current.type) {

        case TOKEN_VAR:
            return parser_parse_var(parser);

        case TOKEN_CONST:
            return parser_parse_const(parser);

        case TOKEN_FN:
            return parser_parse_function(parser);

        case TOKEN_ASYNC:
            return parser_parse_function(parser);

        case TOKEN_EXTERNAL:
            return parser_parse_external(parser);

        case TOKEN_STRUCT:
            return parser_parse_struct(parser);

        case TOKEN_IMPORT:
            return parser_parse_import(parser);

        case TOKEN_FROM:
            return parser_parse_from_import(parser);

        default:
            return parser_parse_statement(parser);
    }
}


/* =========================================================
 * VAR
 * ========================================================= */

ASTNode *parser_parse_var(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_VAR
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_VAR_DECL
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        )) {

        parser_error(
            parser,
            "se esperaba un identificador"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.var_decl.name =
        parser_strdup(
            parser->current.lexeme
        );

    node->data.var_decl.is_private =
        parser->current.lexeme != NULL &&
        parser->current.lexeme[0] == '_';

    parser_advance(parser);

    if (parser_check(parser, TOKEN_COLON)) {
        parser_advance(parser);
        node->data.var_decl.type =
            parser_parse_type(parser);

        if (node->data.var_decl.type == NULL) {
            ast_destroy(node);
            return NULL;
        }
    }

    if (!parser_expect(
            parser,
            TOKEN_ASSIGN
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.var_decl.value =
        parser_parse_expression(parser);

    if (node->data.var_decl.value == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * STRUCT
 *
 * struct Punto:
 *     var x:float
 *     var y:float
 * ========================================================= */

ASTNode *parser_parse_struct(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_STRUCT
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_STRUCT_DECL
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        )) {

        parser_error(
            parser,
            "se esperaba el nombre del struct"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.struct_decl.name =
        parser_strdup(
            parser->current.lexeme
        );

    parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_INDENT
        )) {

        ast_destroy(node);
        return NULL;
    }

    while (
        !parser_check(
            parser,
            TOKEN_DEDENT
        ) &&
        !parser_check(
            parser,
            TOKEN_EOF
        )
    ) {

        if (parser_check(
                parser,
                TOKEN_NEWLINE
            )) {

            parser_advance(parser);
            continue;
        }

        if (!parser_expect(
                parser,
                TOKEN_VAR
            )) {

            ast_destroy(node);
            return NULL;
        }

        ASTNode *field =
            parser_create_node(
                parser,
                AST_VAR_DECL
            );

        if (field == NULL) {

            ast_destroy(node);
            return NULL;
        }

        if (!parser_check(
                parser,
                TOKEN_IDENTIFIER
            )) {

            parser_error(
                parser,
                "se esperaba el nombre del campo"
            );

            ast_destroy(field);
            ast_destroy(node);
            return NULL;
        }

        field->data.var_decl.name =
            parser_strdup(
                parser->current.lexeme
            );

        field->data.var_decl.is_private =
            parser->current.lexeme != NULL &&
            parser->current.lexeme[0] == '_';

        parser_advance(parser);

        if (!parser_expect(
                parser,
                TOKEN_COLON
            )) {

            ast_destroy(field);
            ast_destroy(node);
            return NULL;
        }

        field->data.var_decl.type =
            parser_parse_type(parser);

        if (field->data.var_decl.type == NULL) {

            ast_destroy(field);
            ast_destroy(node);
            return NULL;
        }

        if (parser_check(
                parser,
                TOKEN_NEWLINE
            )) {

            parser_advance(parser);
        }

        ast_node_list_add(
            &node->data.struct_decl.fields,
            field
        );
    }

    if (!parser_expect(
            parser,
            TOKEN_DEDENT
        )) {

        ast_destroy(node);
        return NULL;
    }

    return node;
}


/* =========================================================
 * CONST
 * ========================================================= */

ASTNode *parser_parse_const(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_CONST
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_CONST_DECL
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        )) {

        parser_error(
            parser,
            "se esperaba un identificador"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.const_decl.name =
        parser_strdup(
            parser->current.lexeme
        );

    node->data.const_decl.is_private =
        parser->current.lexeme != NULL &&
        parser->current.lexeme[0] == '_';

    parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.const_decl.type =
        parser_parse_type(parser);

    if (node->data.const_decl.type == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_ASSIGN
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.const_decl.value =
        parser_parse_expression(parser);

    if (node->data.const_decl.value == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * FUNCIÓN
 * ========================================================= */

/*
 * Cabecera compartida por `fn nombre(...)` y
 * `external "ruta.c" fn nombre(...)`: nombre, parámetros
 * y tipo de retorno. NO espera `:` ni cuerpo.
 *
 * Si falla, destruye el nodo y devuelve NULL.
 */
static ASTNode *parser_parse_function_head(
    Parser *parser,
    ASTNode *node
)
{
    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        )) {

        parser_error(
            parser,
            "se esperaba el nombre de la funcion"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.function_decl.name =
        parser_strdup(
            parser->current.lexeme
        );

    node->data.function_decl.is_private =
        parser->current.lexeme != NULL &&
        parser->current.lexeme[0] == '_';

    parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_LPAREN
        )) {

        ast_destroy(node);
        return NULL;
    }

    /*
     * Parámetros.
     */
    while (
        !parser_check(
            parser,
            TOKEN_RPAREN
        ) &&
        !parser_check(
            parser,
            TOKEN_EOF
        )
    ) {

        /*
         * `...` — variadic C (solo en external).
         * Se marca en el nodo y se sale del bucle.
         */
        if (parser_check(parser, TOKEN_DOT)) {

            parser_advance(parser);

            if (parser_check(parser, TOKEN_DOT)) {

                parser_advance(parser);

                if (parser_check(parser, TOKEN_DOT)) {

                    parser_advance(parser);
                    node->data.function_decl.is_variadic = 1;
                    break;
                }
            }

            parser_error(
                parser,
                "se esperaba un parametro"
            );

            ast_destroy(node);
            return NULL;
        }

        if (!parser_check(
                parser,
                TOKEN_IDENTIFIER
            )) {

            parser_error(
                parser,
                "se esperaba un parametro"
            );

            ast_destroy(node);
            return NULL;
        }

        ASTParameter *parameter =
            ast_parameter_create(
                parser_strdup(
                    parser->current.lexeme
                ),
                NULL
            );

        if (parameter == NULL) {

            parser_error(
                parser,
                "no se pudo crear el parametro"
            );

            ast_destroy(node);
            return NULL;
        }

        parameter->is_private =
            parser->current.lexeme != NULL &&
            parser->current.lexeme[0] == '_';

        parser_advance(parser);

        if (!parser_expect(
                parser,
                TOKEN_COLON
            )) {

            free(parameter->name);
            free(parameter);

            ast_destroy(node);
            return NULL;
        }

        parameter->type =
            parser_parse_type(parser);

        if (parameter->type == NULL) {

            free(parameter->name);
            free(parameter);

            ast_destroy(node);
            return NULL;
        }

        ast_parameter_list_add(
            &node->data.function_decl.parameters,
            parameter
        );

        if (parser_check(
                parser,
                TOKEN_COMMA
            )) {

            parser_advance(parser);

            /*
             * Permitir coma final.
             */
            if (parser_check(
                    parser,
                    TOKEN_RPAREN
                )) {
                break;
            }

        } else {

            break;
        }
    }

    if (!parser_expect(
            parser,
            TOKEN_RPAREN
        )) {

        ast_destroy(node);
        return NULL;
    }

    /*
     * Retorno opcional.
     *
     * fn saludar(nombre:string):
     *
     * equivale a void. `-> void` explícito también se
     * acepta (return_type queda NULL).
     */
    node->data.function_decl.return_type = NULL;

    if (parser_check(
            parser,
            TOKEN_ARROW
        )) {

        parser_advance(parser);

        if (parser_check(
                parser,
                TOKEN_VOID
            )) {

            /* -> void explícito: NULL = sin retorno */
            parser_advance(parser);

        } else {

            node->data.function_decl.return_type =
                parser_parse_type(parser);

            if (node->data.function_decl.return_type == NULL) {

                ast_destroy(node);
                return NULL;
            }
        }
    }

    return node;
}


ASTNode *parser_parse_function(
    Parser *parser
)
{
    int is_async = 0;

    if (parser_check(parser, TOKEN_ASYNC)) {

        parser_advance(parser);
        is_async = 1;
    }

    if (!parser_expect(
            parser,
            TOKEN_FN
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_FUNCTION_DECL
        );

    if (node == NULL)
        return NULL;

    node->data.function_decl.is_async = is_async;

    node =
        parser_parse_function_head(parser, node);

    if (node == NULL)
        return NULL;

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    /*
     * El bloque empieza en la siguiente línea.
     */
    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.function_decl.body =
        parser_parse_block(parser);

    if (node->data.function_decl.body == NULL) {

        ast_destroy(node);
        return NULL;
    }

    return node;
}


/* =========================================================
 * FUNCIÓN EXTERNA (C)
 *
 * external "ruta/al/archivo.c" fn suma(a:int, b:int) -> int
 *
 * Declaración sin cuerpo: la implementación está en el
 * fichero C, que el compilador compila y enlaza aparte.
 * La firma se valida igual que la de una función normal.
 * ========================================================= */

ASTNode *parser_parse_external(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_EXTERNAL
        ))
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_STRING_LITERAL
        )) {

        parser_error(
            parser,
            "se esperaba la ruta del fichero C entre comillas"
        );

        return NULL;
    }

    const char *lexeme =
        parser->current.lexeme;

    size_t length =
        lexeme != NULL ? strlen(lexeme) : 0;

    /*
     * Quitar las comillas de la ruta.
     */
    char *path = NULL;

    if (length >= 2) {

        path = malloc(length - 1);

        if (path != NULL) {
            memcpy(path, lexeme + 1, length - 2);
            path[length - 2] = '\0';
        }
    }

    if (path == NULL || path[0] == '\0') {

        free(path);

        parser_error(
            parser,
            "se esperaba la ruta del fichero C entre comillas"
        );

        return NULL;
    }

    parser_advance(parser);

    ASTNode *node =
        parser_create_node(
            parser,
            AST_FUNCTION_DECL
        );

    if (node == NULL) {

        free(path);
        return NULL;
    }

    node->data.function_decl.external_path = path;

    if (!parser_expect(
            parser,
            TOKEN_FN
        )) {

        ast_destroy(node);
        return NULL;
    }

    node =
        parser_parse_function_head(parser, node);

    if (node == NULL)
        return NULL;

    /*
     * Modificadores de propiedad del string devuelto:
     * `-> string owned` o `-> string static`.
     * Solo validos si el retorno es string (la semántica
     * lo comprueba).
     */
    if (parser_check(parser, TOKEN_OWNED)) {

        node->data.function_decl.external_string_owned = 1;
        parser_advance(parser);

    } else if (parser_check(parser, TOKEN_STATIC)) {

        node->data.function_decl.external_string_static = 1;
        parser_advance(parser);
    }

    /*
     * Fin de línea: la declaración no tiene cuerpo.
     */
    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);

    } else if (!parser_check(parser, TOKEN_EOF)) {

        parser_error(
            parser,
            "se esperaba el final de la linea"
        );

        ast_destroy(node);
        return NULL;
    }

    return node;
}


/* =========================================================
 * IMPORT
 * ========================================================= */

ASTNode *parser_parse_import(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_IMPORT
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_IMPORT
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        ) &&
        !parser_check(
            parser,
            TOKEN_STRING
        )) {

        parser_error(
            parser,
            "se esperaba el nombre del modulo"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.import_decl.module =
        parser_strdup(
            parser->current.lexeme
        );

    parser_advance(parser);

    /*
     * import matematicas as math
     */
    if (parser_check(
            parser,
            TOKEN_AS
        )) {

        parser_advance(parser);

        if (!parser_check(
                parser,
                TOKEN_IDENTIFIER
            )) {

            parser_error(
                parser,
                "se esperaba un alias"
            );

            ast_destroy(node);
            return NULL;
        }

        node->data.import_decl.alias =
            parser_strdup(
                parser->current.lexeme
            );

        parser_advance(parser);
    }

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * FROM IMPORT
 * ========================================================= */

ASTNode *parser_parse_from_import(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_FROM
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_FROM_IMPORT
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        ) &&
        !parser_check(
            parser,
            TOKEN_STRING
        )) {

        parser_error(
            parser,
            "se esperaba el nombre del modulo"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.from_import.module =
        parser_strdup(
            parser->current.lexeme
        );

    parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_IMPORT
        )) {

        ast_destroy(node);
        return NULL;
    }

    while (
        !parser_check(
            parser,
            TOKEN_NEWLINE
        ) &&
        !parser_check(
            parser,
            TOKEN_EOF
        )
    ) {

        if (!parser_check(
                parser,
                TOKEN_IDENTIFIER
            )) {

            parser_error(
                parser,
                "se esperaba un nombre para importar"
            );

            ast_destroy(node);
            return NULL;
        }

        ASTNode *name =
            parser_create_node(
                parser,
                AST_IDENTIFIER
            );

        if (name == NULL) {

            ast_destroy(node);
            return NULL;
        }

        name->data.identifier.name =
            parser_strdup(
                parser->current.lexeme
            );

        parser_advance(parser);

        ast_node_list_add(
            &node->data.from_import.names,
            name
        );

        if (parser_check(
                parser,
                TOKEN_COMMA
            )) {

            parser_advance(parser);
            continue;
        }

        break;
    }

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * SENTENCIAS
 * ========================================================= */

ASTNode *parser_parse_statement(
    Parser *parser
)
{
    if (parser == NULL)
        return NULL;

    switch (parser->current.type) {

        case TOKEN_IF:
            return parser_parse_if(parser);

        case TOKEN_WHILE:
            return parser_parse_while(parser);

        case TOKEN_FOR:
            return parser_parse_for(parser);

        case TOKEN_RETURN:
            return parser_parse_return(parser);

        case TOKEN_TRY:
            return parser_parse_try(parser);

        case TOKEN_BREAK:
            return parser_parse_break(parser);

        case TOKEN_CONTINUE:
            return parser_parse_continue(parser);

        case TOKEN_PASS:
            return parser_parse_pass(parser);

        default:
            break;
    }

    ASTNode *expression =
        parser_parse_expression(parser);

    if (expression == NULL)
        return NULL;

    ASTNode *statement =
        parser_create_node(
            parser,
            AST_EXPRESSION_STMT
        );

    if (statement == NULL) {

        ast_destroy(expression);
        return NULL;
    }

    statement->data.expression_stmt.expression =
        expression;

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return statement;
}


/* =========================================================
 * IF
 * ========================================================= */

ASTNode *parser_parse_if(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_IF
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_IF
        );

    if (node == NULL)
        return NULL;

    node->data.if_stmt.condition =
        parser_parse_expression(parser);

    if (node->data.if_stmt.condition == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.if_stmt.then_branch =
        parser_parse_block(parser);

    if (node->data.if_stmt.then_branch == NULL) {

        ast_destroy(node);
        return NULL;
    }

    node->data.if_stmt.else_branch = NULL;

    /*
     * elif cond: ...  es azúcar sintáctico: cada rama se
     * convierte en un AST_IF anidado dentro de else_branch.
     * La semántica y el codegen no cambian.
     */

    ASTNode *tail = node;

    while (parser_check(
            parser,
            TOKEN_ELIF
        )) {

        parser_advance(parser);

        ASTNode *branch =
            parser_create_node(
                parser,
                AST_IF
            );

        if (branch == NULL) {

            ast_destroy(node);
            return NULL;
        }

        branch->data.if_stmt.condition =
            parser_parse_expression(parser);

        if (branch->data.if_stmt.condition == NULL) {

            ast_destroy(branch);
            ast_destroy(node);
            return NULL;
        }

        if (!parser_expect(
                parser,
                TOKEN_COLON
            )) {

            ast_destroy(branch);
            ast_destroy(node);
            return NULL;
        }

        if (!parser_expect(
                parser,
                TOKEN_NEWLINE
            )) {

            ast_destroy(branch);
            ast_destroy(node);
            return NULL;
        }

        branch->data.if_stmt.then_branch =
            parser_parse_block(parser);

        if (branch->data.if_stmt.then_branch == NULL) {

            ast_destroy(branch);
            ast_destroy(node);
            return NULL;
        }

        branch->data.if_stmt.else_branch = NULL;

        tail->data.if_stmt.else_branch = branch;
        tail = branch;
    }

    if (parser_check(
            parser,
            TOKEN_ELSE
        )) {

        parser_advance(parser);

        if (!parser_expect(
                parser,
                TOKEN_COLON
            )) {

            ast_destroy(node);
            return NULL;
        }

        if (!parser_expect(
                parser,
                TOKEN_NEWLINE
            )) {

            ast_destroy(node);
            return NULL;
        }

        tail->data.if_stmt.else_branch =
            parser_parse_block(parser);

        if (tail->data.if_stmt.else_branch == NULL) {

            ast_destroy(node);
            return NULL;
        }
    }

    return node;
}


/* =========================================================
 * WHILE
 * ========================================================= */

ASTNode *parser_parse_while(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_WHILE
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_WHILE
        );

    if (node == NULL)
        return NULL;

    node->data.while_stmt.condition =
        parser_parse_expression(parser);

    if (node->data.while_stmt.condition == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.while_stmt.body =
        parser_parse_block(parser);

    if (node->data.while_stmt.body == NULL) {

        ast_destroy(node);
        return NULL;
    }

    return node;
}


/* =========================================================
 * FOR
 * ========================================================= */

ASTNode *parser_parse_for(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_FOR
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_FOR
        );

    if (node == NULL)
        return NULL;

    if (!parser_check(
            parser,
            TOKEN_IDENTIFIER
        )) {

        parser_error(
            parser,
            "se esperaba la variable del for"
        );

        ast_destroy(node);
        return NULL;
    }

    node->data.for_stmt.variable =
        parser_strdup(
            parser->current.lexeme
        );

    parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_IN
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.for_stmt.iterable =
        parser_parse_expression(parser);

    if (node->data.for_stmt.iterable == NULL) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.for_stmt.body =
        parser_parse_block(parser);

    if (node->data.for_stmt.body == NULL) {

        ast_destroy(node);
        return NULL;
    }

    return node;
}


/* =========================================================
 * RETURN
 * ========================================================= */

ASTNode *parser_parse_return(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_RETURN
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_RETURN
        );

    if (node == NULL)
        return NULL;

    /*
     * return vacío.
     */
    if (parser_check(
            parser,
            TOKEN_NEWLINE
        ) ||
        parser_check(
            parser,
            TOKEN_DEDENT
        ) ||
        parser_check(
            parser,
            TOKEN_EOF
        )) {

        node->data.return_stmt.value = NULL;

    } else {

        node->data.return_stmt.value =
            parser_parse_expression(parser);

        if (node->data.return_stmt.value == NULL) {

            ast_destroy(node);
            return NULL;
        }
    }

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * TRY / CATCH
 * ========================================================= */

ASTNode *parser_parse_try(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_TRY
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_TRY
        );

    if (node == NULL)
        return NULL;

    if (!parser_expect(
            parser,
            TOKEN_COLON
        )) {

        ast_destroy(node);
        return NULL;
    }

    if (!parser_expect(
            parser,
            TOKEN_NEWLINE
        )) {

        ast_destroy(node);
        return NULL;
    }

    node->data.try_stmt.try_body =
        parser_parse_block(parser);

    if (node->data.try_stmt.try_body == NULL) {

        ast_destroy(node);
        return NULL;
    }

    /*
     * catch <nombre>:
     *     bloque
     *
     * Única cláusula permitida, sin anotación de tipo
     * (claude.md §15). Es obligatoria: el diseño solo
     * define el par try/catch, no un try suelto.
     *
     * El nombre todavía no se declara (el lenguaje no
     * define el tipo del error): usarlo dentro del catch
     * produce el error semántico "name not defined".
     */

    if (!parser_check(parser, TOKEN_CATCH)) {

        parser_error(
            parser,
            "expected catch after try"
        );

        ast_destroy(node);
        return NULL;
    }

    parser_advance(parser);

    if (!parser_check(parser, TOKEN_IDENTIFIER)) {

        parser_error(
            parser,
            "se esperaba un nombre para catch"
        );

        ast_destroy(node);
        return NULL;
    }

    char *error_name =
        parser_strdup(parser->current.lexeme);

    if (error_name == NULL) {

        parser_error(
            parser,
            "no se pudo reservar memoria para el AST"
        );

        ast_destroy(node);
        return NULL;
    }

    parser_advance(parser);

    if (!parser_expect(parser, TOKEN_COLON) ||
        !parser_expect(parser, TOKEN_NEWLINE)) {

        free(error_name);
        ast_destroy(node);
        return NULL;
    }

    ASTNode *catch_node =
        parser_create_node(
            parser,
            AST_CATCH
        );

    if (catch_node == NULL) {

        free(error_name);
        ast_destroy(node);
        return NULL;
    }

    catch_node->data.catch_stmt.error_name = error_name;
    catch_node->data.catch_stmt.error_type = NULL;
    catch_node->data.catch_stmt.body =
        parser_parse_block(parser);

    if (catch_node->data.catch_stmt.body == NULL) {

        ast_destroy(catch_node);
        ast_destroy(node);
        return NULL;
    }

    ast_node_list_add(
        &node->data.try_stmt.catches,
        catch_node
    );

    return node;
}


/* =========================================================
 * BREAK
 * ========================================================= */

ASTNode *parser_parse_break(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_BREAK
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_BREAK
        );

    if (node == NULL)
        return NULL;

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * CONTINUE
 * ========================================================= */

ASTNode *parser_parse_continue(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_CONTINUE
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_CONTINUE
        );

    if (node == NULL)
        return NULL;

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * PASS
 * ========================================================= */

ASTNode *parser_parse_pass(
    Parser *parser
)
{
    if (!parser_expect(
            parser,
            TOKEN_PASS
        ))
        return NULL;

    ASTNode *node =
        parser_create_node(
            parser,
            AST_PASS
        );

    if (node == NULL)
        return NULL;

    if (parser_check(
            parser,
            TOKEN_NEWLINE
        )) {

        parser_advance(parser);
    }

    return node;
}


/* =========================================================
 * BLOQUE
 * ========================================================= */

ASTNode *parser_parse_block(
    Parser *parser
)
{
    if (parser == NULL)
        return NULL;

    ASTNode *block =
        parser_create_node(
            parser,
            AST_BLOCK
        );

    if (block == NULL)
        return NULL;

    /*
     * El lexer debe producir INDENT.
     *
     * Antes del primer statement del bloque se admiten
     * líneas en blanco o de comentario: son NEWLINE, y sin
     * esto un bloque que empiece por un comentario daba
     * "se esperaba INDENT".
     */
    while (parser_check(parser, TOKEN_NEWLINE))
        parser_advance(parser);

    if (!parser_expect(
            parser,
            TOKEN_INDENT
        )) {

        ast_destroy(block);
        return NULL;
    }

    while (
        !parser_check(
            parser,
            TOKEN_DEDENT
        ) &&
        !parser_check(
            parser,
            TOKEN_EOF
        )
    ) {

        if (parser_check(
                parser,
                TOKEN_NEWLINE
            )) {

            parser_advance(parser);
            continue;
        }

        ASTNode *statement =
            parser_parse_declaration(parser);

        if (statement != NULL) {

            ast_node_list_add(
                &block->data.block.statements,
                statement
            );

        } else {

            parser_synchronize(parser);
        }
    }

    if (parser_check(
            parser,
            TOKEN_DEDENT
        )) {

        parser_advance(parser);
    }

    return block;
}


/* =========================================================
 * EXPRESIONES
 * ========================================================= */

ASTNode *parser_parse_expression(
    Parser *parser
)
{
    return parser_parse_assignment(parser);
}


/* =========================================================
 * ASSIGNMENT
 * ========================================================= */

ASTNode *parser_parse_assignment(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_or(parser);

    if (left == NULL)
        return NULL;

    ASTOperator operator;

    switch (parser->current.type) {

        case TOKEN_ASSIGN:
            operator = AST_OP_ASSIGN;
            break;

        case TOKEN_PLUS_ASSIGN:
            operator = AST_OP_ADD_ASSIGN;
            break;

        case TOKEN_MINUS_ASSIGN:
            operator = AST_OP_SUB_ASSIGN;
            break;

        case TOKEN_STAR_ASSIGN:
            operator = AST_OP_MUL_ASSIGN;
            break;

        case TOKEN_SLASH_ASSIGN:
            operator = AST_OP_DIV_ASSIGN;
            break;

        default:
            return left;
    }

    parser_advance(parser);

    ASTNode *value =
        parser_parse_assignment(parser);

    if (value == NULL) {

        ast_destroy(left);
        return NULL;
    }

    ASTNode *assignment =
        parser_create_node(
            parser,
            AST_ASSIGN
        );

    if (assignment == NULL) {

        ast_destroy(left);
        ast_destroy(value);
        return NULL;
    }

    assignment->data.assignment.target =
        left;

    assignment->data.assignment.operator =
        operator;

    assignment->data.assignment.value =
        value;

    return assignment;
}


/* =========================================================
 * OR
 * ========================================================= */

ASTNode *parser_parse_or(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_and(parser);

    if (left == NULL)
        return NULL;

    while (
        parser_check(
            parser,
            TOKEN_OR
        )
    ) {

        parser_advance(parser);

        ASTNode *right =
            parser_parse_and(parser);

        if (right == NULL) {

            ast_destroy(left);
            return NULL;
        }

        ASTNode *node =
            parser_create_node(
                parser,
                AST_BINARY
            );

        if (node == NULL) {

            ast_destroy(left);
            ast_destroy(right);
            return NULL;
        }

        node->data.binary.operator =
            AST_OP_OR;

        node->data.binary.left = left;
        node->data.binary.right = right;

        left = node;
    }

    return left;
}


/* =========================================================
 * AND
 * ========================================================= */

ASTNode *parser_parse_and(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_comparison(parser);

    if (left == NULL)
        return NULL;

    while (
        parser_check(
            parser,
            TOKEN_AND
        )
    ) {

        parser_advance(parser);

        ASTNode *right =
            parser_parse_comparison(parser);

        if (right == NULL) {

            ast_destroy(left);
            return NULL;
        }

        ASTNode *node =
            parser_create_node(
                parser,
                AST_BINARY
            );

        if (node == NULL) {

            ast_destroy(left);
            ast_destroy(right);
            return NULL;
        }

        node->data.binary.operator =
            AST_OP_AND;

        node->data.binary.left = left;
        node->data.binary.right = right;

        left = node;
    }

    return left;
}


/* =========================================================
 * COMPARACIÓN
 * ========================================================= */

ASTNode *parser_parse_comparison(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_term(parser);

    if (left == NULL)
        return NULL;

    while (1) {

        ASTOperator operator;

        switch (parser->current.type) {

            case TOKEN_EQUAL:
                operator = AST_OP_EQUAL;
                break;

            case TOKEN_NOT_EQUAL:
                operator = AST_OP_NOT_EQUAL;
                break;

            case TOKEN_GREATER:
                operator = AST_OP_GREATER;
                break;

            case TOKEN_LESS:
                operator = AST_OP_LESS;
                break;

            case TOKEN_GREATER_EQUAL:
                operator = AST_OP_GREATER_EQUAL;
                break;

            case TOKEN_LESS_EQUAL:
                operator = AST_OP_LESS_EQUAL;
                break;

            default:
                return left;
        }

        parser_advance(parser);

        ASTNode *right =
            parser_parse_term(parser);

        if (right == NULL) {

            ast_destroy(left);
            return NULL;
        }

        ASTNode *node =
            parser_create_node(
                parser,
                AST_BINARY
            );

        if (node == NULL) {

            ast_destroy(left);
            ast_destroy(right);
            return NULL;
        }

        node->data.binary.operator =
            operator;

        node->data.binary.left = left;
        node->data.binary.right = right;

        left = node;
    }
}


/* =========================================================
 * TERM
 * ========================================================= */

ASTNode *parser_parse_term(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_factor(parser);

    if (left == NULL)
        return NULL;

    while (
        parser_check(
            parser,
            TOKEN_PLUS
        ) ||
        parser_check(
            parser,
            TOKEN_MINUS
        )
    ) {

        ASTOperator operator;

        if (parser->current.type ==
            TOKEN_PLUS) {

            operator = AST_OP_ADD;

        } else {

            operator = AST_OP_SUB;
        }

        parser_advance(parser);

        ASTNode *right =
            parser_parse_factor(parser);

        if (right == NULL) {

            ast_destroy(left);
            return NULL;
        }

        ASTNode *node =
            parser_create_node(
                parser,
                AST_BINARY
            );

        if (node == NULL) {

            ast_destroy(left);
            ast_destroy(right);
            return NULL;
        }

        node->data.binary.operator =
            operator;

        node->data.binary.left = left;
        node->data.binary.right = right;

        left = node;
    }

    return left;
}


/* =========================================================
 * FACTOR
 * ========================================================= */

ASTNode *parser_parse_factor(
    Parser *parser
)
{
    ASTNode *left =
        parser_parse_unary(parser);

    if (left == NULL)
        return NULL;

    while (
        parser_check(
            parser,
            TOKEN_STAR
        ) ||
        parser_check(
            parser,
            TOKEN_SLASH
        ) ||
        parser_check(
            parser,
            TOKEN_FLOOR_DIV
        ) ||
        parser_check(
            parser,
            TOKEN_PERCENT
        )
    ) {

        ASTOperator operator;

        switch (parser->current.type) {

            case TOKEN_STAR:
                operator = AST_OP_MUL;
                break;

            case TOKEN_SLASH:
                operator = AST_OP_DIV;
                break;
            case TOKEN_FLOOR_DIV:
                operator = AST_OP_FLOOR_DIV;
                break;
            default:
                operator = AST_OP_MOD;
                break;
        }

        parser_advance(parser);

        ASTNode *right =
            parser_parse_unary(parser);

        if (right == NULL) {

            ast_destroy(left);
            return NULL;
        }

        ASTNode *node =
            parser_create_node(
                parser,
                AST_BINARY
            );

        if (node == NULL) {

            ast_destroy(left);
            ast_destroy(right);
            return NULL;
        }

        node->data.binary.operator =
            operator;

        node->data.binary.left = left;
        node->data.binary.right = right;

        left = node;
    }

    return left;
}


/* =========================================================
 * UNARY
 * ========================================================= */

ASTNode *parser_parse_unary(
    Parser *parser
)
{
    /*
     * await es prefijo unario: await tarea.
     */
    if (parser_check(parser, TOKEN_AWAIT)) {

        ASTNode *node =
            parser_create_node(
                parser,
                AST_AWAIT
            );

        if (node == NULL)
            return NULL;

        parser_advance(parser);

        ASTNode *operand =
            parser_parse_unary(parser);

        if (operand == NULL) {

            ast_destroy(node);
            return NULL;
        }

        node->data.await_expr.operand = operand;

        return node;
    }

    if (
        parser_check(
            parser,
            TOKEN_MINUS
        ) ||
        parser_check(
            parser,
            TOKEN_NOT
        ) ||
        parser_check(
            parser,
            TOKEN_INCREMENT
        ) ||
        parser_check(
            parser,
            TOKEN_DECREMENT
        )
    ) {

        ASTOperator operator;

        if (parser->current.type ==
            TOKEN_MINUS) {

            operator = AST_OP_NEGATE;

        } else if (parser->current.type ==
                   TOKEN_NOT) {

            operator = AST_OP_NOT;

        } else if (parser->current.type ==
                   TOKEN_INCREMENT) {

            operator = AST_OP_INCREMENT;

        } else {

            operator = AST_OP_DECREMENT;
        }

        parser_advance(parser);

        ASTNode *operand =
            parser_parse_unary(parser);

        if (operand == NULL)
            return NULL;

        ASTNode *node =
            parser_create_node(
                parser,
                AST_UNARY
            );

        if (node == NULL) {

            ast_destroy(operand);
            return NULL;
        }

        node->data.unary.operator =
            operator;

        node->data.unary.operand =
            operand;
        node->data.unary.is_postfix = 0;

        return node;
    }

    return parser_parse_primary(parser);
}


/* =========================================================
 * PRIMARY
 * ========================================================= */

static void parser_skip_collection_layout(Parser *parser)
{
    while (parser_check(parser, TOKEN_NEWLINE) ||
           parser_check(parser, TOKEN_INDENT) ||
           parser_check(parser, TOKEN_DEDENT)) {
        parser_advance(parser);
    }
}


static ASTNode *parser_parse_map_literal(
    Parser *parser,
    TokenType closing_token,
    ASTNode *first_key
)
{
    ASTNode *node = parser_create_node(
        parser,
        AST_MAP_LITERAL
    );

    if (node == NULL) {
        ast_destroy(first_key);
        return NULL;
    }

    parser_skip_collection_layout(parser);

    if (first_key == NULL &&
        parser_check(parser, closing_token)) {
        parser_advance(parser);
        return node;
    }

    while (1) {
        ASTNode *key = first_key;
        first_key = NULL;

        if (key == NULL) {
            key = parser_parse_expression(parser);
            if (key == NULL) {
                ast_destroy(node);
                return NULL;
            }
        }

        if (!parser_expect(parser, TOKEN_COLON)) {
            ast_destroy(key);
            ast_destroy(node);
            return NULL;
        }

        ASTNode *value = parser_parse_expression(parser);
        if (value == NULL) {
            ast_destroy(key);
            ast_destroy(node);
            return NULL;
        }

        ASTMapEntry *entry = ast_map_entry_create(key, value);
        if (entry == NULL) {
            ast_destroy(key);
            ast_destroy(value);
            ast_destroy(node);
            return NULL;
        }

        ast_map_entry_list_add(
            &node->data.map_literal.entries,
            entry
        );

        parser_skip_collection_layout(parser);

        if (!parser_check(parser, TOKEN_COMMA))
            break;

        parser_advance(parser);
        parser_skip_collection_layout(parser);

        if (parser_check(parser, closing_token))
            break;
    }

    parser_skip_collection_layout(parser);

    if (!parser_expect(parser, closing_token)) {
        ast_destroy(node);
        return NULL;
    }

    return node;
}


ASTNode *parser_parse_primary(
    Parser *parser
)
{
    if (parser == NULL)
        return NULL;

    ASTNode *node = NULL;

    switch (parser->current.type) {

        /* -------------------------------------------------
         * INTEGER
         * ------------------------------------------------- */

        case TOKEN_INT_LITERAL:
        {
            node = parser_create_node(
                parser,
                AST_INT_LITERAL
            );

            if (node == NULL)
                return NULL;

            node->data.int_literal.value =
                strtoll(
                    parser->current.lexeme,
                    NULL,
                    10
                );

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * FLOAT
         * ------------------------------------------------- */

        case TOKEN_FLOAT_LITERAL:
        {
            node = parser_create_node(
                parser,
                AST_FLOAT_LITERAL
            );

            if (node == NULL)
                return NULL;

            node->data.float_literal.value =
                strtod(
                    parser->current.lexeme,
                    NULL
                );

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * STRING
         * ------------------------------------------------- */

        case TOKEN_STRING_LITERAL:
        {
            node = parser_create_node(
                parser,
                AST_STRING_LITERAL
            );

            if (node == NULL)
                return NULL;

            const char *lexeme =
                parser->current.lexeme;

            /*
             * Quitar las comillas y resolver los escapes.
             */
            char *value =
                parser_string_literal_value(lexeme);

            if (value == NULL) {

                ast_destroy(node);
                return NULL;
            }

            node->data.string_literal.value =
                value;

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * CHAR
         * ------------------------------------------------- */

        case TOKEN_CHAR_LITERAL:
        {
            node = parser_create_node(
                parser,
                AST_CHAR_LITERAL
            );

            if (node == NULL)
                return NULL;

            const char *lexeme =
                parser->current.lexeme;

            size_t length =
                strlen(lexeme);

            if (length >= 3) {

                if (lexeme[1] == '\\' &&
                    length >= 4) {

                    switch (lexeme[2]) {

                        case 'n':
                            node->data.char_literal.value =
                                '\n';
                            break;

                        case 'r':
                            node->data.char_literal.value =
                                '\r';
                            break;

                        case 't':
                            node->data.char_literal.value =
                                '\t';
                            break;

                        case '\\':
                            node->data.char_literal.value =
                                '\\';
                            break;

                        case '\'':
                            node->data.char_literal.value =
                                '\'';
                            break;

                        case '"':
                            node->data.char_literal.value =
                                '"';
                            break;

                        case '0':
                            node->data.char_literal.value =
                                '\0';
                            break;

                        default:
                            node->data.char_literal.value =
                                lexeme[2];
                            break;
                    }

                } else {

                    node->data.char_literal.value =
                        lexeme[1];
                }
            }

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * TRUE
         * ------------------------------------------------- */

        case TOKEN_TRUE:
        case TOKEN_FALSE:
        {
            node = parser_create_node(
                parser,
                AST_BOOLEAN_LITERAL
            );

            if (node == NULL)
                return NULL;

            node->data.boolean_literal.value =
                parser->current.type ==
                TOKEN_TRUE;

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * NULL
         * ------------------------------------------------- */

        case TOKEN_NULL:
        {
            node = parser_create_node(
                parser,
                AST_NULL_LITERAL
            );

            if (node == NULL)
                return NULL;

            parser_advance(parser);
            break;
        }


        /* -------------------------------------------------
         * F-STRING
         *
         * f"texto {expr} texto"
         *
         * Solo se admiten expresiones restringidas dentro
         * de las llaves: una primaria con sus sufijos
         * (identificador, miembro, índice y llamada).
         * Los operadores binarios y unarios no forman
         * parte de la interpolación.
         * ------------------------------------------------- */

        case TOKEN_FSTRING_START:
        {
            node = parser_create_node(
                parser,
                AST_FSTRING
            );

            if (node == NULL)
                return NULL;

            parser_advance(parser);

            while (1) {

                /* Trozo literal. */
                if (parser_check(
                        parser,
                        TOKEN_FSTRING_TEXT
                    )) {

                    char *value =
                        parser_fstring_text_value(
                            parser->current.lexeme
                        );

                    if (value == NULL) {

                        ast_destroy(node);
                        return NULL;
                    }

                    ASTFStringSegment *segment =
                        ast_fstring_segment_create(
                            0,
                            value,
                            NULL
                        );

                    if (segment == NULL) {

                        free(value);
                        ast_destroy(node);
                        return NULL;
                    }

                    ast_fstring_segment_list_add(
                        &node->data.fstring.segments,
                        segment
                    );

                    parser_advance(parser);
                    continue;
                }

                /* Interpolación {expr}. */
                if (parser_check(
                        parser,
                        TOKEN_LBRACE
                    )) {

                    parser_advance(parser);

                    if (parser_check(
                            parser,
                            TOKEN_RBRACE
                        ) ||
                        parser_check(
                            parser,
                            TOKEN_EOF
                        ) ||
                        parser_check(
                            parser,
                            TOKEN_FSTRING_END
                        )) {

                        parser_error(
                            parser,
                            "se esperaba una expresion "
                            "dentro de las llaves del "
                            "f-string"
                        );

                        ast_destroy(node);
                        return NULL;
                    }

                    ASTNode *expr =
                        parser_parse_primary(parser);

                    if (expr == NULL) {

                        ast_destroy(node);
                        return NULL;
                    }

                    if (!parser_expect(
                            parser,
                            TOKEN_RBRACE
                        )) {

                        ast_destroy(expr);
                        ast_destroy(node);
                        return NULL;
                    }

                    ASTFStringSegment *segment =
                        ast_fstring_segment_create(
                            1,
                            NULL,
                            expr
                        );

                    if (segment == NULL) {

                        ast_destroy(expr);
                        ast_destroy(node);
                        return NULL;
                    }

                    ast_fstring_segment_list_add(
                        &node->data.fstring.segments,
                        segment
                    );

                    continue;
                }

                /* Cierre del f-string. */
                if (parser_check(
                        parser,
                        TOKEN_FSTRING_END
                    )) {

                    parser_advance(parser);
                    break;
                }

                parser_error_at(
                    parser,
                    "f-string no cerrado",
                    &parser->current
                );

                ast_destroy(node);
                return NULL;
            }

            break;
        }


        /* -------------------------------------------------
         * IDENTIFICADOR
         * ------------------------------------------------- */

        case TOKEN_IDENTIFIER:
        case TOKEN_STRING:
        {
            node = parser_create_node(
                parser,
                AST_IDENTIFIER
            );

            if (node == NULL)
                return NULL;

            node->data.identifier.name =
                parser_strdup(
                    parser->current.lexeme
                );

            parser_advance(parser);

            break;
        }


        /* -------------------------------------------------
         * PARÉNTESIS
         * ------------------------------------------------- */

        case TOKEN_LPAREN:
        {
            parser_advance(parser);

            node =
                parser_parse_expression(parser);

            if (node == NULL)
                return NULL;

            if (!parser_expect(
                    parser,
                    TOKEN_RPAREN
                )) {

                ast_destroy(node);
                return NULL;
            }

            break;
        }

        case TOKEN_LBRACE:
        {
            parser_advance(parser);
            node = parser_parse_map_literal(
                parser,
                TOKEN_RBRACE,
                NULL
            );
            if (node == NULL)
                return NULL;
            break;
        }


        /* -------------------------------------------------
         * LISTA / MAPA
         * ------------------------------------------------- */

        case TOKEN_LBRACKET:
        {
            node = NULL;

            parser_advance(parser);
            parser_skip_collection_layout(parser);

            /*
             * [] vacío.
             */
            if (parser_check(
                    parser,
                    TOKEN_RBRACKET
                )) {

                node = parser_create_node(
                    parser,
                    AST_LIST_LITERAL
                );

                if (node == NULL)
                    return NULL;

                parser_advance(parser);
                break;
            }

            /*
             * Primer elemento.
             */
            ASTNode *first =
                parser_parse_expression(parser);

            if (first == NULL)
                return NULL;

            /*
             * [clave: valor]
             *
             * Detectamos map por ':' después
             * del primer elemento.
             */
            if (parser_check(
                    parser,
                    TOKEN_COLON
                )) {

                node = parser_create_node(
                    parser,
                    AST_MAP_LITERAL
                );

                if (node == NULL) {

                    ast_destroy(first);
                    return NULL;
                }

                parser_advance(parser);

                ASTNode *value =
                    parser_parse_expression(parser);

                if (value == NULL) {

                    ast_destroy(first);
                    ast_destroy(node);
                    return NULL;
                }

                ASTMapEntry *entry =
                    ast_map_entry_create(
                        first,
                        value
                    );

                if (entry == NULL) {

                    ast_destroy(first);
                    ast_destroy(value);
                    ast_destroy(node);
                    return NULL;
                }

                ast_map_entry_list_add(
                    &node->data.map_literal.entries,
                    entry
                );

                while (
                    parser_check(
                        parser,
                        TOKEN_COMMA
                    )
                ) {

                    parser_advance(parser);
                    parser_skip_collection_layout(parser);

                    if (parser_check(
                            parser,
                            TOKEN_RBRACKET
                        )) {
                        break;
                    }

                    ASTNode *key =
                        parser_parse_expression(parser);

                    if (key == NULL) {

                        ast_destroy(node);
                        return NULL;
                    }

                    if (!parser_expect(
                            parser,
                            TOKEN_COLON
                        )) {

                        ast_destroy(key);
                        ast_destroy(node);
                        return NULL;
                    }

                    ASTNode *map_value =
                        parser_parse_expression(parser);

                    if (map_value == NULL) {

                        ast_destroy(key);
                        ast_destroy(node);
                        return NULL;
                    }

                    ASTMapEntry *map_entry =
                        ast_map_entry_create(
                            key,
                            map_value
                        );

                    if (map_entry == NULL) {

                        ast_destroy(key);
                        ast_destroy(map_value);
                        ast_destroy(node);
                        return NULL;
                    }

                    ast_map_entry_list_add(
                        &node->data.map_literal.entries,
                        map_entry
                    );
                }

            } else {

                /*
                 * Lista.
                 */
                node = parser_create_node(
                    parser,
                    AST_LIST_LITERAL
                );

                if (node == NULL) {

                    ast_destroy(first);
                    return NULL;
                }

                ast_node_list_add(
                    &node->data.list_literal.elements,
                    first
                );

                while (
                    parser_check(
                        parser,
                        TOKEN_COMMA
                    )
                ) {

                    parser_advance(parser);
                    parser_skip_collection_layout(parser);

                    if (parser_check(
                            parser,
                            TOKEN_RBRACKET
                        )) {
                        break;
                    }

                    ASTNode *element =
                        parser_parse_expression(parser);

                    if (element == NULL) {

                        ast_destroy(node);
                        return NULL;
                    }

                    ast_node_list_add(
                        &node->data.list_literal.elements,
                        element
                    );
                }
            }

            parser_skip_collection_layout(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_RBRACKET
                )) {

                ast_destroy(node);
                return NULL;
            }

            break;
        }


        default:
            parser_error_at(
                parser,
                "se esperaba una expresion",
                &parser->current
            );

            return NULL;
    }


    /*
     * Postfix:
     *
     * funcion()
     * lista[0]
     * objeto.miembro
     */
    while (1) {

        /* ---------------------------------------------
         * LLAMADA
         * --------------------------------------------- */

        if (parser_check(
                parser,
                TOKEN_LPAREN
            )) {

            parser_advance(parser);

            ASTNode *call =
                parser_create_node(
                    parser,
                    AST_CALL
                );

            if (call == NULL) {

                ast_destroy(node);
                return NULL;
            }

            call->data.call.callee =
                node;

            while (
                !parser_check(
                    parser,
                    TOKEN_RPAREN
                ) &&
                !parser_check(
                    parser,
                    TOKEN_EOF
                )
            ) {

                ASTNode *argument =
                    parser_parse_expression(parser);

                if (argument == NULL) {

                    ast_destroy(call);
                    return NULL;
                }

                ast_node_list_add(
                    &call->data.call.arguments,
                    argument
                );

                if (parser_check(
                        parser,
                        TOKEN_COMMA
                    )) {

                    parser_advance(parser);
                    continue;
                }

                break;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_RPAREN
                )) {

                ast_destroy(call);
                return NULL;
            }

            node = call;
            continue;
        }


        /* ---------------------------------------------
         * ÍNDICE
         * --------------------------------------------- */

        if (parser_check(
                parser,
                TOKEN_LBRACKET
            )) {

            parser_advance(parser);

            ASTNode *index =
                parser_parse_expression(parser);

            if (index == NULL) {

                ast_destroy(node);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_RBRACKET
                )) {

                ast_destroy(index);
                ast_destroy(node);
                return NULL;
            }

            ASTNode *index_node =
                parser_create_node(
                    parser,
                    AST_INDEX
                );

            if (index_node == NULL) {

                ast_destroy(index);
                ast_destroy(node);
                return NULL;
            }

            index_node->data.index.object =
                node;

            index_node->data.index.index =
                index;

            node = index_node;
            continue;
        }


        /* ---------------------------------------------
         * MIEMBRO
         * --------------------------------------------- */

        if (parser_check(
                parser,
                TOKEN_DOT
            )) {

            parser_advance(parser);

            if (!parser_check(
                    parser,
                    TOKEN_IDENTIFIER
                )) {

                parser_error(
                    parser,
                    "se esperaba el nombre del miembro"
                );

                ast_destroy(node);
                return NULL;
            }

            ASTNode *member =
                parser_create_node(
                    parser,
                    AST_MEMBER
                );

            if (member == NULL) {

                ast_destroy(node);
                return NULL;
            }

            member->data.member.object =
                node;

            member->data.member.member =
                parser_strdup(
                    parser->current.lexeme
                );

            parser_advance(parser);

            node = member;
            continue;
        }

        if (parser_check(parser, TOKEN_INCREMENT) ||
            parser_check(parser, TOKEN_DECREMENT)) {
            ASTNode *unary =
                parser_create_node(parser, AST_UNARY);
            if (unary == NULL) {
                ast_destroy(node);
                return NULL;
            }
            unary->data.unary.operator =
                parser->current.type == TOKEN_INCREMENT
                    ? AST_OP_INCREMENT
                    : AST_OP_DECREMENT;
            unary->data.unary.operand = node;
            unary->data.unary.is_postfix = 1;
            parser_advance(parser);
            node = unary;
            continue;
        }

        break;
    }

    return node;
}


/* =========================================================
 * TIPOS
 * ========================================================= */

static void parser_destroy_type(Type *type)
{
    if (type == NULL)
        return;

    parser_destroy_type(type->element_type);
    parser_destroy_type(type->key_type);
    parser_destroy_type(type->value_type);
    parser_destroy_type(type->return_type);

    if (type->parameter_types != NULL) {
        for (int i = 0; i < type->parameter_count; i++)
            parser_destroy_type(type->parameter_types[i]);
    }

    free(type->parameter_types);
    free(type->struct_name);
    free(type);
}


Type *parser_parse_type(
    Parser *parser
)
{
    if (parser == NULL)
        return NULL;

    Type *type = NULL;

    switch (parser->current.type) {

        case TOKEN_INT:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_INT;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;


        case TOKEN_FLOAT:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_FLOAT;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;


        case TOKEN_STRING:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_STRING;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;


        case TOKEN_BOOL:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_BOOL;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;


        case TOKEN_CHAR:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_CHAR;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;

        case TOKEN_I8:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_I8;

            parser_advance(parser);

            return type;

        case TOKEN_I16:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_I16;

            parser_advance(parser);

            return type;

        case TOKEN_I32:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_I32;

            parser_advance(parser);

            return type;

        case TOKEN_U8:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_U8;

            parser_advance(parser);

            return type;

        case TOKEN_U16:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_U16;

            parser_advance(parser);

            return type;

        case TOKEN_U32:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_U32;

            parser_advance(parser);

            return type;

        case TOKEN_U64:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_U64;

            parser_advance(parser);

            return type;

        case TOKEN_SIZE:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_SIZE;

            parser_advance(parser);

            return type;

        case TOKEN_IDENTIFIER:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_STRUCT;
            type->struct_name =
                parser_strdup(parser->current.lexeme);

            if (type->struct_name == NULL) {
                parser_destroy_type(type);
                return NULL;
            }

            parser_advance(parser);

            return type;


        case TOKEN_NULL:

            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_NULL;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            return type;


        /* -------------------------------------------------
         * list<T>
         * ------------------------------------------------- */

        case TOKEN_LIST:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_LIST;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_LESS
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            type->element_type =
                parser_parse_type(parser);

            if (type->element_type == NULL) {

                parser_destroy_type(type);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_GREATER
                )) {

                parser_destroy_type(type);

                return NULL;
            }

            return type;
        }


        /* -------------------------------------------------
         * task<T>
         * ------------------------------------------------- */
        case TOKEN_TASK:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_TASK;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_LESS
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            type->element_type =
                parser_parse_type(parser);

            if (type->element_type == NULL) {

                parser_destroy_type(type);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_GREATER
                )) {

                parser_destroy_type(type);

                return NULL;
            }

            return type;
        }


        /* -------------------------------------------------
         * map<K, V>
         * ------------------------------------------------- */

        case TOKEN_MAP:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_MAP;
            type->element_type = NULL;
            type->key_type = NULL;
            type->value_type = NULL;

            parser_advance(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_LESS
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            type->key_type =
                parser_parse_type(parser);

            if (type->key_type == NULL) {

                parser_destroy_type(type);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_COMMA
                )) {

                parser_destroy_type(type);

                return NULL;
            }

            type->value_type =
                parser_parse_type(parser);

            if (type->value_type == NULL) {

                parser_destroy_type(type);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_GREATER
                )) {

                parser_destroy_type(type);

                return NULL;
            }

            return type;
        }


        /* -------------------------------------------------
         * opaque — handle opaco (void*) para FFI
         * ------------------------------------------------- */

        case TOKEN_OPAQUE:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_OPAQUE;

            parser_advance(parser);

            return type;
        }


        /* -------------------------------------------------
         * callback<T1, T2, ...> -> T
         *
         * Puntero a función C. Los parámetros van entre
         * < >; el retorno va tras `->` (obligatorio).
         * Solo como parámetro de firma externa.
         * ------------------------------------------------- */

        case TOKEN_CALLBACK:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_CALLBACK;

            parser_advance(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_LESS
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            /*
             * Parámetros: lista separada por comas.
             * callback<> -> void admite lista vacía.
             */
            if (!parser_check(
                    parser,
                    TOKEN_GREATER
                )) {

                do {

                    Type *param =
                        parser_parse_type(parser);

                    if (param == NULL) {

                        parser_destroy_type(type);
                        return NULL;
                    }

                    Type **grown =
                        realloc(
                            type->parameter_types,
                            (size_t) (type->parameter_count + 1)
                                * sizeof(Type *)
                        );

                    if (grown == NULL) {

                        parser_destroy_type(param);
                        parser_destroy_type(type);
                        return NULL;
                    }

                    type->parameter_types = grown;
                    type->parameter_types[
                        type->parameter_count
                    ] = param;
                    type->parameter_count++;

                } while (
                    parser_check(
                        parser,
                        TOKEN_COMMA
                    ) && (
                        parser_advance(parser),
                        1
                    )
                );
            }

            if (!parser_expect(
                    parser,
                    TOKEN_GREATER
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            /*
             * Retorno: `-> tipo` obligatorio para poder
             * generar el tipo LLVM del puntero a función.
             */
            if (!parser_expect(
                    parser,
                    TOKEN_ARROW
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            if (parser_check(
                    parser,
                    TOKEN_VOID
                )) {

                /* -> void: return_type queda NULL */
                parser_advance(parser);

            } else {

                type->return_type =
                    parser_parse_type(parser);

                if (type->return_type == NULL) {

                    parser_destroy_type(type);
                    return NULL;
                }
            }

            return type;
        }


        /* -------------------------------------------------
         * buffer<T> — puntero + longitud para FFI.
         * Solo como parámetro de firma externa.
         * ------------------------------------------------- */

        case TOKEN_BUFFER:
        {
            type = calloc(1, sizeof(Type));

            if (type == NULL)
                return NULL;

            type->kind = TYPE_BUFFER;

            parser_advance(parser);

            if (!parser_expect(
                    parser,
                    TOKEN_LESS
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            type->element_type =
                parser_parse_type(parser);

            if (type->element_type == NULL) {

                parser_destroy_type(type);
                return NULL;
            }

            if (!parser_expect(
                    parser,
                    TOKEN_GREATER
                )) {

                parser_destroy_type(type);
                return NULL;
            }

            return type;
        }


        default:
            parser_error(
                parser,
                "se esperaba un tipo"
            );

            return NULL;
    }
}