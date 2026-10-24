#include "lexer.h"
#include "error.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * CONSTANTES
 * ========================================================= */

#define LEXER_INITIAL_PENDING_CAPACITY 16
#define LEXER_MAX_INDENT_DEPTH 256


/* =========================================================
 * PALABRAS CLAVE
 * ========================================================= */

typedef struct {
    const char *word;
    TokenType type;
} Keyword;


static const Keyword keywords[] = {

    /* Variables */
    {"var", TOKEN_VAR},
    {"const", TOKEN_CONST},

    /* Funciones */
    {"fn", TOKEN_FN},
    {"return", TOKEN_RETURN},

    /* Estructuras */
    {"struct", TOKEN_STRUCT},

    /* Concurrencia */
    {"async", TOKEN_ASYNC},
    {"await", TOKEN_AWAIT},

    /* Condicionales */
    {"if", TOKEN_IF},
    {"elif", TOKEN_ELIF},
    {"else", TOKEN_ELSE},

    /* Bucles */
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"in", TOKEN_IN},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},

    /* Sentencia vacía */
    {"pass", TOKEN_PASS},

    /* Manejo de errores */
    {"try", TOKEN_TRY},
    {"catch", TOKEN_CATCH},

    /* Imports */
    {"import", TOKEN_IMPORT},
    {"from", TOKEN_FROM},
    {"as", TOKEN_AS},
    {"external", TOKEN_EXTERNAL},

    /* Valores */
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"null", TOKEN_NULL},

    /* Tipos */
    {"int", TOKEN_INT},
    {"float", TOKEN_FLOAT},
    {"string", TOKEN_STRING},
    {"bool", TOKEN_BOOL},
    {"char", TOKEN_CHAR},
    {"list", TOKEN_LIST},
    {"map", TOKEN_MAP},
    {"task", TOKEN_TASK},

    /* Tipos FFI (solo firmas externas) */
    {"i8", TOKEN_I8},
    {"i16", TOKEN_I16},
    {"i32", TOKEN_I32},
    {"u8", TOKEN_U8},
    {"u16", TOKEN_U16},
    {"u32", TOKEN_U32},
    {"u64", TOKEN_U64},
    {"size", TOKEN_SIZE},
    {"void", TOKEN_VOID},

    /* Tipos FFI fase 2 */
    {"opaque", TOKEN_OPAQUE},
    {"callback", TOKEN_CALLBACK},

    /* Tipos FFI fase 3 */
    {"buffer", TOKEN_BUFFER},

    /* Modificadores de retorno string en external */
    {"owned", TOKEN_OWNED},
    {"static", TOKEN_STATIC},

    /* Operadores lógicos */
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT}
};


#define KEYWORD_COUNT \
    (sizeof(keywords) / sizeof(keywords[0]))


/* =========================================================
 * FUNCIONES INTERNAS
 * ========================================================= */

static char lexer_current(const Lexer *lexer)
{
    if (lexer == NULL)
        return '\0';

    if (lexer->position >= lexer->length)
        return '\0';

    return lexer->source[lexer->position];
}


static char lexer_peek_char(const Lexer *lexer)
{
    if (lexer == NULL)
        return '\0';

    if (lexer->position + 1 >= lexer->length)
        return '\0';

    return lexer->source[lexer->position + 1];
}


static char lexer_peek_char_n(
    const Lexer *lexer,
    int offset
)
{
    if (lexer == NULL)
        return '\0';

    int position = lexer->position + offset;

    if (position < 0 ||
        position >= lexer->length)
        return '\0';

    return lexer->source[position];
}


static void lexer_advance_char(Lexer *lexer)
{
    if (lexer == NULL)
        return;

    if (lexer->position >= lexer->length)
        return;

    char c = lexer->source[lexer->position];

    lexer->position++;

    if (c == '\n') {
        lexer->line = lexer->line + 1;
        lexer->column = 1;
    } else {
        lexer->column = lexer->column + 1;
    }
}


static int lexer_is_at_end(const Lexer *lexer)
{
    if (lexer == NULL)
        return 1;

    return lexer->position >= lexer->length;
}


static Token lexer_make_token(
    Lexer *lexer,
    TokenType type,
    int start,
    int line,
    int column
)
{
    Token token;

    token.type = type;
    token.lexeme = NULL;
    token.line = line;
    token.column = column;
    token.length = lexer->position - start;

    if (token.length > 0) {

        token.lexeme = malloc(
            (size_t)token.length + 1
        );

        if (token.lexeme != NULL) {

            memcpy(
                token.lexeme,
                lexer->source + start,
                (size_t)token.length
            );

            token.lexeme[token.length] = '\0';
        }
    }

    return token;
}


static Token lexer_make_error(
    Lexer *lexer,
    int start,
    int line,
    int column
)
{
    return lexer_make_token(
        lexer,
        TOKEN_ERROR,
        start,
        line,
        column
    );
}


static Token lexer_make_simple_token(
    Lexer *lexer,
    TokenType type
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    lexer_advance_char(lexer);

    return lexer_make_token(
        lexer,
        type,
        start,
        line,
        column
    );
}


/* =========================================================
 * KEYWORDS
 * ========================================================= */

static TokenType lexer_keyword_type(
    const char *text,
    int length
)
{
    for (size_t i = 0;
         i < KEYWORD_COUNT;
         i++) {

        size_t keyword_length =
            strlen(keywords[i].word);

        if ((int)keyword_length != length)
            continue;

        if (strncmp(
                text,
                keywords[i].word,
                (size_t)length
            ) == 0) {

            return keywords[i].type;
        }
    }

    return TOKEN_IDENTIFIER;
}


/* =========================================================
 * IDENTIFICADORES
 * ========================================================= */

static Token lexer_scan_identifier(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    /*
     * Los nombres privados empiezan por '_'.
     */
    if (lexer_current(lexer) == '_')
        lexer_advance_char(lexer);

    /*
     * Después del '_' debe haber un identificador
     * válido.
     */
    if (!isalpha(
            (unsigned char)lexer_current(lexer)
        ) &&
        lexer_current(lexer) != '_') {

        return lexer_make_error(
            lexer,
            start,
            line,
            column
        );
    }

    while (isalnum(
               (unsigned char)lexer_current(lexer)
           ) ||
           lexer_current(lexer) == '_') {

        lexer_advance_char(lexer);
    }

    int length = lexer->position - start;

    TokenType type = lexer_keyword_type(
        lexer->source + start,
        length
    );

    return lexer_make_token(
        lexer,
        type,
        start,
        line,
        column
    );
}


/* =========================================================
 * NÚMEROS
 * ========================================================= */

static Token lexer_scan_number(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    while (isdigit(
        (unsigned char)lexer_current(lexer)
    )) {
        lexer_advance_char(lexer);
    }

    /*
     * Float.
     */
    if (lexer_current(lexer) == '.') {

        /*
         * Si después del punto hay un número,
         * es float.
         */
        if (isdigit(
            (unsigned char)lexer_peek_char(lexer)
        )) {

            lexer_advance_char(lexer);

            while (isdigit(
                (unsigned char)lexer_current(lexer)
            )) {
                lexer_advance_char(lexer);
            }

            /*
             * No permitimos algo como:
             *
             * 12.34.56
             */
            if (lexer_current(lexer) == '.') {

                while (
                    isdigit(
                        (unsigned char)
                        lexer_current(lexer)
                    )
                ) {
                    lexer_advance_char(lexer);
                }

                return lexer_make_error(
                    lexer,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_token(
                lexer,
                TOKEN_FLOAT_LITERAL,
                start,
                line,
                column
            );
        }
    }

    /*
     * Un número seguido de letras no es válido.
     *
     * 123abc
     */
    if (isalpha(
        (unsigned char)lexer_current(lexer)
    ) ||
        lexer_current(lexer) == '_') {

        while (
            isalnum(
                (unsigned char)
                lexer_current(lexer)
            ) ||
            lexer_current(lexer) == '_' ||
            lexer_current(lexer) == '.'
        ) {
            lexer_advance_char(lexer);
        }

        return lexer_make_error(
            lexer,
            start,
            line,
            column
        );
    }

    return lexer_make_token(
        lexer,
        TOKEN_INT_LITERAL,
        start,
        line,
        column
    );
}


/* =========================================================
 * STRING
 * ========================================================= */

static Token lexer_scan_string(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    /*
     * Consumir "
     */
    lexer_advance_char(lexer);

    while (!lexer_is_at_end(lexer)) {

        char c = lexer_current(lexer);

        /*
         * String terminado.
         */
        if (c == '"') {

            lexer_advance_char(lexer);

            return lexer_make_token(
                lexer,
                TOKEN_STRING_LITERAL,
                start,
                line,
                column
            );
        }

        /*
         * Un salto de línea dentro de un string
         * sin cerrar es un error.
         */
        if (c == '\n' || c == '\r') {

            return lexer_make_error(
                lexer,
                start,
                line,
                column
            );
        }

        /*
         * Escape.
         */
        if (c == '\\') {

            lexer_advance_char(lexer);

            if (lexer_is_at_end(lexer))
                break;

            char escaped =
                lexer_current(lexer);

            switch (escaped) {

                case 'n':
                case 'r':
                case 't':
                case '\\':
                case '"':
                case '\'':
                case '0':
                    lexer_advance_char(lexer);
                    break;

                default:

                    lexer_advance_char(lexer);

                    return lexer_make_error(
                        lexer,
                        start,
                        line,
                        column
                    );
            }

            continue;
        }

        lexer_advance_char(lexer);
    }

    /*
     * EOF antes de cerrar.
     */
    return lexer_make_error(
        lexer,
        start,
        line,
        column
    );
}


/* =========================================================
 * F-STRING
 *
 * f"texto {expr} texto"
 *
 * El lexer produce varios tokens:
 *
 *     FSTRING_START  (f")
 *     FSTRING_TEXT   (trozo literal, crudo)
 *     LBRACE, expresión, RBRACE
 *     FSTRING_TEXT
 *     FSTRING_END    (")
 *
 * {{ produce una llave literal y }} una de cierre; una
 * llave sola que no abre/cierra interpolación es un error
 * (E1013). Un f-string sin cerrar es E1012.
 * ========================================================= */

/*
 * Error de f-string reportado aquí mismo (mismo patrón que
 * E1011): el token se devuelve con longitud 0 para que el
 * parser lo reconozca como ya reportado y no imprima los
 * errores de sintaxis que vendrían en cascada.
 */
static Token lexer_fstring_error(
    Lexer *lexer,
    ErrorKind kind,
    const char *message,
    int line,
    int column
)
{
    const char *detail = NULL;
    const char *suggestion = NULL;

    if (kind == ERROR_LEX_UNTERMINATED_FSTRING) {

        detail =
            "el f-string abierto con f\" nunca se cierra";
        suggestion =
            "cierra el f-string con una comilla doble";

    } else if (kind == ERROR_LEX_INVALID_FSTRING) {

        detail =
            "una llave sola debe abrir una interpolacion {expr}";
        suggestion =
            "usa {{ o }} si querias una llave literal";
    }

    error_render(
        error_kind_class(kind),
        ERROR_ERROR,
        lexer->filename != NULL
            ? lexer->filename
            : "?",
        lexer->source,
        line,
        column,
        1,
        message,
        detail,
        suggestion
    );

    lexer->fstring_mode = 0;

    Token token;

    token.type = TOKEN_ERROR;
    token.lexeme = NULL;
    token.line = line;
    token.column = column;
    token.length = 0;

    return token;
}


/*
 * Inicio del f-string: se consumen la 'f' y la comilla de
 * apertura y se pasa al modo texto.
 */
static Token lexer_scan_fstring(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    lexer_advance_char(lexer);   /* f */
    lexer_advance_char(lexer);   /* " */

    lexer->fstring_mode = 1;

    return lexer_make_token(
        lexer,
        TOKEN_FSTRING_START,
        start,
        line,
        column
    );
}


/*
 * Texto literal de un f-string.
 *
 * Se emite un trozo hasta el siguiente delimitador; los
 * delimitadores (", {, }) se emiten como tokens propios y
 * los `{{` / `}}` se dejan crudos para que los resuelva el
 * parser (igual que los escapes, ver §20).
 */
static Token lexer_scan_fstring_text(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    while (!lexer_is_at_end(lexer)) {

        char c = lexer_current(lexer);

        /*
         * Cierre del f-string.
         */
        if (c == '"') {

            if (lexer->position == start) {

                lexer_advance_char(lexer);

                lexer->fstring_mode = 0;

                return lexer_make_token(
                    lexer,
                    TOKEN_FSTRING_END,
                    start,
                    line,
                    column
                );
            }

            /*
             * Hay texto pendiente: se emite primero; en la
             * siguiente llamada start == position y la
             * comilla se consumirá como cierre.
             */
            return lexer_make_token(
                lexer,
                TOKEN_FSTRING_TEXT,
                start,
                line,
                column
            );
        }

        /*
         * Inicio de interpolación.
         */
        if (c == '{') {

            /*
             * {{ -> llave literal.
             */
            if (lexer_peek_char(lexer) == '{') {

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);
                continue;
            }

            if (lexer->position == start) {

                lexer_advance_char(lexer);

                lexer->fstring_mode = 2;

                return lexer_make_token(
                    lexer,
                    TOKEN_LBRACE,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_token(
                lexer,
                TOKEN_FSTRING_TEXT,
                start,
                line,
                column
            );
        }

        /*
         * Llave de cierre.
         */
        if (c == '}') {

            /*
             * }} -> llave literal.
             */
            if (lexer_peek_char(lexer) == '}') {

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);
                continue;
            }

            return lexer_fstring_error(
                lexer,
                ERROR_LEX_INVALID_FSTRING,
                "llave de cierre sin abrir en el f-string",
                line,
                column
            );
        }

        /*
         * Escape: misma lista que los strings normales.
         */
        if (c == '\\') {

            lexer_advance_char(lexer);

            if (lexer_is_at_end(lexer))
                break;

            char escaped =
                lexer_current(lexer);

            switch (escaped) {

                case 'n':
                case 'r':
                case 't':
                case '\\':
                case '"':
                case '\'':
                case '0':
                    lexer_advance_char(lexer);
                    break;

                default:

                    lexer_advance_char(lexer);

                    return lexer_make_error(
                        lexer,
                        start,
                        line,
                        column
                    );
            }

            continue;
        }

        /*
         * Salto de línea: el f-string no está cerrado.
         */
        if (c == '\n' || c == '\r') {

            return lexer_fstring_error(
                lexer,
                ERROR_LEX_UNTERMINATED_FSTRING,
                "f-string sin cerrar",
                line,
                column
            );
        }

        lexer_advance_char(lexer);
    }

    /*
     * EOF antes de cerrar.
     */
    return lexer_fstring_error(
        lexer,
        ERROR_LEX_UNTERMINATED_FSTRING,
        "f-string sin cerrar",
        line,
        column
    );
}


/* =========================================================
 * CHAR
 * ========================================================= */

static Token lexer_scan_char(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    /*
     * Consumir '
     */
    lexer_advance_char(lexer);

    if (lexer_is_at_end(lexer)) {

        return lexer_make_error(
            lexer,
            start,
            line,
            column
        );
    }

    /*
     * Escape.
     */
    if (lexer_current(lexer) == '\\') {

        lexer_advance_char(lexer);

        if (lexer_is_at_end(lexer)) {

            return lexer_make_error(
                lexer,
                start,
                line,
                column
            );
        }

        char escaped =
            lexer_current(lexer);

        switch (escaped) {

            case 'n':
            case 'r':
            case 't':
            case '\\':
            case '\'':
            case '"':
            case '0':
                lexer_advance_char(lexer);
                break;

            default:

                lexer_advance_char(lexer);

                return lexer_make_error(
                    lexer,
                    start,
                    line,
                    column
                );
        }

    } else {

        /*
         * Un carácter normal.
         */
        if (lexer_current(lexer) == '\n' ||
            lexer_current(lexer) == '\r') {

            return lexer_make_error(
                lexer,
                start,
                line,
                column
            );
        }

        lexer_advance_char(lexer);
    }

    /*
     * Debe existir la comilla de cierre.
     */
    if (lexer_current(lexer) != '\'') {

        /*
         * Consumimos hasta encontrar cierre o
         * salto de línea para evitar dejar al
         * lexer en un estado extraño.
         */
        while (
            !lexer_is_at_end(lexer) &&
            lexer_current(lexer) != '\n' &&
            lexer_current(lexer) != '\r' &&
            lexer_current(lexer) != '\''
        ) {
            lexer_advance_char(lexer);
        }

        if (lexer_current(lexer) == '\'')
            lexer_advance_char(lexer);

        return lexer_make_error(
            lexer,
            start,
            line,
            column
        );
    }

    lexer_advance_char(lexer);

    return lexer_make_token(
        lexer,
        TOKEN_CHAR_LITERAL,
        start,
        line,
        column
    );
}


/* =========================================================
 * INDENTACIÓN
 * ========================================================= */

static int lexer_count_indentation(
    Lexer *lexer
)
{
    int indentation = 0;

    while (!lexer_is_at_end(lexer)) {

        char c = lexer_current(lexer);

        if (c == ' ') {

            indentation++;
            lexer_advance_char(lexer);

        } else if (c == '\t') {

            /*
             * Un tab equivale a 4 espacios.
             */
            indentation += 4;
            lexer_advance_char(lexer);

        } else {

            break;
        }
    }

    return indentation;
}


static int lexer_pending_push(
    Lexer *lexer,
    Token token
)
{
    if (lexer == NULL)
        return 0;

    if (lexer->pending_count >=
        lexer->pending_capacity) {

        int new_capacity;

        if (lexer->pending_capacity == 0)
            new_capacity =
                LEXER_INITIAL_PENDING_CAPACITY;
        else
            new_capacity =
                lexer->pending_capacity * 2;

        Token *new_tokens = realloc(
            lexer->pending_tokens,
            sizeof(Token) *
                (size_t)new_capacity
        );

        if (new_tokens == NULL)
            return 0;

        lexer->pending_tokens = new_tokens;
        lexer->pending_capacity = new_capacity;
    }

    lexer->pending_tokens[
        lexer->pending_count
    ] = token;

    lexer->pending_count++;

    return 1;
}


static Token lexer_pending_pop(
    Lexer *lexer
)
{
    Token token;

    token.type = TOKEN_EOF;
    token.lexeme = NULL;
    token.line = 0;
    token.column = 0;
    token.length = 0;

    if (lexer == NULL ||
        lexer->pending_count <= 0) {

        return token;
    }

    token = lexer->pending_tokens[0];

    for (int i = 1;
         i < lexer->pending_count;
         i++) {

        lexer->pending_tokens[i - 1] =
            lexer->pending_tokens[i];
    }

    lexer->pending_count--;

    return token;
}


/* =========================================================
 * PROCESAR INDENTACIÓN
 * ========================================================= */

static int lexer_process_indentation(
    Lexer *lexer
)
{
    if (lexer == NULL)
        return 0;

    int indentation =
        lexer_count_indentation(lexer);

    int current_indent =
        lexer->indent_stack[
            lexer->indent_top
        ];

    /*
     * Línea vacía.
     */
    if (lexer_current(lexer) == '\n' ||
        lexer_current(lexer) == '\r' ||
        lexer_current(lexer) == '#' ||
        lexer_is_at_end(lexer)) {

        return 0;
    }

    /*
     * Aumenta indentación.
     */
    if (indentation > current_indent) {

        if (lexer->indent_top + 1 >=
            LEXER_MAX_INDENT_DEPTH) {

            return 0;
        }

        lexer->indent_top++;

        lexer->indent_stack[
            lexer->indent_top
        ] = indentation;

        Token token = lexer_make_token(
            lexer,
            TOKEN_INDENT,
            lexer->position,
            lexer->line,
            lexer->column
        );

        token.length = 0;

        return lexer_pending_push(
            lexer,
            token
        );
    }

    /*
     * Reduce indentación.
     */
    if (indentation < current_indent) {

        while (
            lexer->indent_top > 0 &&
            indentation <
                lexer->indent_stack[
                    lexer->indent_top
                ]
        ) {

            lexer->indent_top--;

            Token token;

            token.type = TOKEN_DEDENT;
            token.lexeme = NULL;
            token.line = lexer->line;
            token.column = lexer->column;
            token.length = 0;

            if (!lexer_pending_push(
                    lexer,
                    token
                )) {

                return 0;
            }
        }

        /*
         * Si no coincide con ningún nivel anterior,
         * tenemos una indentación inválida.
         */
        if (indentation !=
            lexer->indent_stack[
                lexer->indent_top
            ]) {

            Token token;

            token.type = TOKEN_ERROR;
            token.lexeme = NULL;
            token.line = lexer->line;
            token.column = lexer->column;
            token.length = 0;

            lexer_pending_push(
                lexer,
                token
            );

            return 1;
        }
    }

    return lexer->pending_count > 0;
}


/* =========================================================
 * COMENTARIOS Y ESPACIOS
 * ========================================================= */

static void lexer_skip_comment(
    Lexer *lexer
)
{
    while (!lexer_is_at_end(lexer) &&
           lexer_current(lexer) != '\n' &&
           lexer_current(lexer) != '\r') {

        lexer_advance_char(lexer);
    }
}


/*
 * Comentario de varias lineas: #! ... !#
 *
 * No se anida: el primer !# cierra el comentario. Si el
 * fichero termina antes se reporta E1011 aqui mismo y se
 * devuelve 0 para que el token de error no repita el
 * diagnostico.
 */
static int lexer_skip_block_comment(
    Lexer *lexer,
    int *error_line,
    int *error_column
)
{
    int line = lexer->line;
    int column = lexer->column;

    /* consumimos #! */
    lexer_advance_char(lexer);
    lexer_advance_char(lexer);

    while (!lexer_is_at_end(lexer)) {

        if (lexer_current(lexer) == '!' &&
            lexer_peek_char(lexer) == '#') {

            lexer_advance_char(lexer);
            lexer_advance_char(lexer);

            return 1;
        }

        lexer_advance_char(lexer);
    }

    if (error_line != NULL)
        *error_line = line;

    if (error_column != NULL)
        *error_column = column;

    error_render(
        "SyntaxError",
        ERROR_ERROR,
        lexer->filename != NULL
            ? lexer->filename
            : "?",
        lexer->source,
        line,
        column,
        2,
        "comentario de multiple lineas sin cerrar",
        "el comentario abierto con #! no tiene cierre",
        "cierra el comentario con !#"
    );

    return 0;
}


static int lexer_skip_spaces(
    Lexer *lexer,
    int *error_line,
    int *error_column
)
{
    while (!lexer_is_at_end(lexer)) {

        char c = lexer_current(lexer);

        if (c == ' ' || c == '\t') {

            lexer_advance_char(lexer);

        } else if (c == '#' &&
                   lexer_peek_char(lexer) == '!') {

            if (!lexer_skip_block_comment(
                    lexer,
                    error_line,
                    error_column)) {

                return 0;
            }

        } else if (c == '#') {

            lexer_skip_comment(lexer);

        } else {

            break;
        }
    }

    return 1;
}


/* =========================================================
 * NUEVA LÍNEA
 * ========================================================= */

static void lexer_consume_newline(
    Lexer *lexer
)
{
    if (lexer_current(lexer) == '\r') {

        lexer_advance_char(lexer);

        if (lexer_current(lexer) == '\n')
            lexer_advance_char(lexer);

    } else {

        lexer_advance_char(lexer);
    }
}


static Token lexer_scan_newline(
    Lexer *lexer
)
{
    int start = lexer->position;
    int line = lexer->line;
    int column = lexer->column;

    lexer_consume_newline(lexer);

    return lexer_make_token(
        lexer,
        TOKEN_NEWLINE,
        start,
        line,
        column
    );
}


/*
 * EOF con un paréntesis abierto: se reporta aquí (mismo
 * patrón que el comentario de varias líneas y el f-string)
 * y se devuelve un token de error con longitud 0 para que
 * el parser no repita los errores de sintaxis que vendrían
 * en cascada.
 */
static Token lexer_unterminated_paren(
    Lexer *lexer
)
{
    error_render(
        "SyntaxError",
        ERROR_ERROR,
        lexer->filename != NULL
            ? lexer->filename
            : "?",
        lexer->source,
        lexer->line,
        lexer->column,
        1,
        "parentesis sin cerrar",
        "el parentesis abierto con ( no tiene cierre",
        "cierra el parentesis con )"
    );

    /*
     * Solo se reporta una vez.
     */
    lexer->paren_depth = 0;

    Token token;

    token.type = TOKEN_ERROR;
    token.lexeme = NULL;
    token.line = lexer->line;
    token.column = lexer->column;
    token.length = 0;

    return token;
}


/* =========================================================
 * EOF
 * ========================================================= */

static void lexer_generate_final_dedents(
    Lexer *lexer
)
{
    if (lexer == NULL)
        return;

    while (lexer->indent_top > 0) {

        lexer->indent_top--;

        Token token;

        token.type = TOKEN_DEDENT;
        token.lexeme = NULL;
        token.line = lexer->line;
        token.column = lexer->column;
        token.length = 0;

        lexer_pending_push(
            lexer,
            token
        );
    }
}


/* =========================================================
 * CONSTRUCTOR
 * ========================================================= */

Lexer *lexer_create(
    const char *source
)
{
    Lexer *lexer = malloc(sizeof(Lexer));

    if (lexer == NULL)
        return NULL;

    lexer->source =
        source != NULL
            ? source
            : "";

    lexer->filename = NULL;

    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;

    lexer->length =
        (int)strlen(lexer->source);

    lexer->indent_top = 0;

    for (int i = 0;
         i < LEXER_MAX_INDENT_DEPTH;
         i++) {

        lexer->indent_stack[i] = 0;
    }

    lexer->fstring_mode = 0;

    lexer->paren_depth = 0;

    lexer->pending_tokens = NULL;
    lexer->pending_count = 0;
    lexer->pending_capacity = 0;

    return lexer;
}


/* =========================================================
 * DESTRUCTOR
 * ========================================================= */

void lexer_destroy(
    Lexer *lexer
)
{
    if (lexer == NULL)
        return;

    for (int i = 0;
         i < lexer->pending_count;
         i++) {

        token_destroy(
            &lexer->pending_tokens[i]
        );
    }

    free(lexer->pending_tokens);

    lexer->pending_tokens = NULL;
    lexer->pending_count = 0;
    lexer->pending_capacity = 0;

    free(lexer);
}


void lexer_set_filename(
    Lexer *lexer,
    const char *filename
)
{
    if (lexer == NULL)
        return;

    lexer->filename = filename;
}


/* =========================================================
 * SIGUIENTE TOKEN
 * ========================================================= */

Token lexer_next_token(
    Lexer *lexer
)
{
    Token empty;

    empty.type = TOKEN_EOF;
    empty.lexeme = NULL;
    empty.line = 0;
    empty.column = 0;
    empty.length = 0;

    if (lexer == NULL)
        return empty;


    /*
     * Punto de reinicio: se usa cuando un salto de línea
     * queda dentro de unos paréntesis sin cerrar y no
     * produce token (continuación de línea).
     */
restart:


    /*
     * Primero devolver tokens pendientes.
     */
    if (lexer->pending_count > 0)
        return lexer_pending_pop(lexer);


    /*
     * Texto de un f-string en curso: se escanea antes que
     * la indentación, los espacios y los comentarios
     * porque todo eso es literal dentro del f-string.
     */
    if (lexer->fstring_mode == 1)
        return lexer_scan_fstring_text(lexer);


    /*
     * EOF dentro de una interpolación {expr}.
     */
    if (lexer_is_at_end(lexer) &&
        lexer->fstring_mode == 2) {

        return lexer_fstring_error(
            lexer,
            ERROR_LEX_UNTERMINATED_FSTRING,
            "f-string sin cerrar",
            lexer->line,
            lexer->column
        );
    }


    /*
     * EOF.
     */
    if (lexer_is_at_end(lexer)) {

        if (lexer->paren_depth > 0)
            return lexer_unterminated_paren(lexer);

        if (lexer->indent_top > 0) {

            lexer_generate_final_dedents(
                lexer
            );

            if (lexer->pending_count > 0)
                return lexer_pending_pop(
                    lexer
                );
        }

        empty.type = TOKEN_EOF;
        empty.line = lexer->line;
        empty.column = lexer->column;

        return empty;
    }


    /*
     * Al comienzo de una línea:
     *
     * calcular indentación.
     *
     * Dentro de unos paréntesis sin cerrar la
     * indentación se ignora, igual que en Python.
     */
    if (lexer->column == 1 &&
        lexer->paren_depth == 0) {

        if (lexer_process_indentation(lexer)) {

            if (lexer->pending_count > 0)
                return lexer_pending_pop(
                    lexer
                );
        }
    }


    /*
     * Espacios y comentarios.
     */
    int comment_line = 0;
    int comment_column = 0;

    if (!lexer_skip_spaces(
            lexer,
            &comment_line,
            &comment_column)) {

        /*
         * El diagnostico (E1011) ya se imprimio; se
         * devuelve un token de error sin lexeme para que
         * el parser no lo repita.
         */
        empty.type = TOKEN_ERROR;
        empty.lexeme = NULL;
        empty.line = comment_line;
        empty.column = comment_column;
        empty.length = 0;

        return empty;
    }


    /*
     * Volvemos a comprobar EOF.
     */
    if (lexer_is_at_end(lexer)) {

        if (lexer->paren_depth > 0)
            return lexer_unterminated_paren(lexer);

        if (lexer->indent_top > 0) {

            lexer_generate_final_dedents(
                lexer
            );

            if (lexer->pending_count > 0)
                return lexer_pending_pop(
                    lexer
                );
        }

        empty.type = TOKEN_EOF;
        empty.line = lexer->line;
        empty.column = lexer->column;

        return empty;
    }


    /*
     * Comentario después de espacios.
     */
    if (lexer_current(lexer) == '#') {

        lexer_skip_comment(lexer);

        if (!lexer_is_at_end(lexer))
            return lexer_scan_newline(lexer);

        empty.type = TOKEN_EOF;
        empty.line = lexer->line;
        empty.column = lexer->column;

        return empty;
    }


    char c = lexer_current(lexer);


    /* =====================================================
     * IDENTIFICADORES
     * ===================================================== */

    if (isalpha((unsigned char)c) ||
        c == '_') {

        /*
         * f"..." (f-string). Solo fuera de una
         * interpolación: dentro de {expr} no se anidan.
         */
        if (lexer->fstring_mode == 0 &&
            c == 'f' &&
            lexer_peek_char(lexer) == '"') {

            return lexer_scan_fstring(
                lexer
            );
        }

        return lexer_scan_identifier(
            lexer
        );
    }


    /* =====================================================
     * NÚMEROS
     * ===================================================== */

    if (isdigit((unsigned char)c)) {

        return lexer_scan_number(
            lexer
        );
    }


    /* =====================================================
     * STRING
     * ===================================================== */

    if (c == '"') {

        return lexer_scan_string(
            lexer
        );
    }


    /* =====================================================
     * CARÁCTER
     * ===================================================== */

    if (c == '\'') {

        return lexer_scan_char(
            lexer
        );
    }


    /* =====================================================
     * NUEVA LÍNEA
     * ===================================================== */

    if (c == '\n' ||
        c == '\r') {

        /*
         * Salto de línea dentro de {expr}: el f-string
         * quedó sin cerrar.
         */
        if (lexer->fstring_mode == 2) {

            return lexer_fstring_error(
                lexer,
                ERROR_LEX_UNTERMINATED_FSTRING,
                "f-string sin cerrar",
                lexer->line,
                lexer->column
            );
        }

        /*
         * Continuación de línea: dentro de unos
         * paréntesis el salto de línea no separa
         * sentencias y no se emite TOKEN_NEWLINE.
         */
        if (lexer->paren_depth > 0) {

            lexer_consume_newline(lexer);
            goto restart;
        }

        return lexer_scan_newline(
            lexer
        );
    }


    /* =====================================================
     * OPERADORES
     * ===================================================== */

    switch (c) {

        case '+':

            if (lexer_peek_char(lexer) == '+') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_INCREMENT,
                    start,
                    line,
                    column
                );
            }

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_PLUS_ASSIGN,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_PLUS
            );


        case '-':

            if (lexer_peek_char(lexer) == '-') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_DECREMENT,
                    start,
                    line,
                    column
                );
            }

            if (lexer_peek_char(lexer) == '>') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_ARROW,
                    start,
                    line,
                    column
                );
            }

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_MINUS_ASSIGN,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_MINUS
            );


        case '*':

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_STAR_ASSIGN,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_STAR
            );


        case '/':

            if (lexer_peek_char(lexer) == '/') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_FLOOR_DIV,
                    start,
                    line,
                    column
                );
            }

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_SLASH_ASSIGN,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_SLASH
            );


        case '%':

            return lexer_make_simple_token(
                lexer,
                TOKEN_PERCENT
            );


        case '=':

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_EQUAL,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_ASSIGN
            );


        case '!':

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_NOT_EQUAL,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_ERROR
            );


        case '>':

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_GREATER_EQUAL,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_GREATER
            );


        case '<':

            if (lexer_peek_char(lexer) == '=') {

                int start = lexer->position;
                int line = lexer->line;
                int column = lexer->column;

                lexer_advance_char(lexer);
                lexer_advance_char(lexer);

                return lexer_make_token(
                    lexer,
                    TOKEN_LESS_EQUAL,
                    start,
                    line,
                    column
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_LESS
            );


        /* =================================================
         * SÍMBOLOS
         * ================================================= */

        case ':':

            return lexer_make_simple_token(
                lexer,
                TOKEN_COLON
            );


        case ',':

            return lexer_make_simple_token(
                lexer,
                TOKEN_COMMA
            );


        case '.':

            return lexer_make_simple_token(
                lexer,
                TOKEN_DOT
            );


        case '(':

            lexer->paren_depth++;

            return lexer_make_simple_token(
                lexer,
                TOKEN_LPAREN
            );


        case ')':

            if (lexer->paren_depth > 0)
                lexer->paren_depth--;

            return lexer_make_simple_token(
                lexer,
                TOKEN_RPAREN
            );


        case '[':

            return lexer_make_simple_token(
                lexer,
                TOKEN_LBRACKET
            );


        case '{':

            return lexer_make_simple_token(
                lexer,
                TOKEN_LBRACE
            );


        case ']':

            return lexer_make_simple_token(
                lexer,
                TOKEN_RBRACKET
            );


        case '}':

            /* Cierre de un mapa o de {expr} en un f-string. */
            if (lexer->fstring_mode == 2) {

                lexer->fstring_mode = 1;

                return lexer_make_simple_token(
                    lexer,
                    TOKEN_RBRACE
                );
            }

            return lexer_make_simple_token(
                lexer,
                TOKEN_RBRACE
            );


        default:
            break;
    }


    /*
     * Carácter desconocido.
     */
    {
        int start = lexer->position;
        int line = lexer->line;
        int column = lexer->column;

        lexer_advance_char(lexer);

        return lexer_make_error(
            lexer,
            start,
            line,
            column
        );
    }
}


/* =========================================================
 * PEEK
 * ========================================================= */

Token lexer_peek_token(
    Lexer *lexer
)
{
    if (lexer == NULL) {

        Token token;

        token.type = TOKEN_EOF;
        token.lexeme = NULL;
        token.line = 0;
        token.column = 0;
        token.length = 0;

        return token;
    }

    /*
     * Guardamos todo el estado del lexer.
     */
    int position = lexer->position;
    int line = lexer->line;
    int column = lexer->column;
    int indent_top = lexer->indent_top;
    int fstring_mode = lexer->fstring_mode;
    int paren_depth = lexer->paren_depth;

    int indent_stack[
        LEXER_MAX_INDENT_DEPTH
    ];

    memcpy(
        indent_stack,
        lexer->indent_stack,
        sizeof(indent_stack)
    );

    Token *pending_backup = NULL;

    if (lexer->pending_count > 0) {

        pending_backup = malloc(
            sizeof(Token) *
            (size_t)lexer->pending_count
        );

        if (pending_backup == NULL) {

            Token token;

            token.type = TOKEN_ERROR;
            token.lexeme = NULL;
            token.line = lexer->line;
            token.column = lexer->column;
            token.length = 0;

            return token;
        }

        memcpy(
            pending_backup,
            lexer->pending_tokens,
            sizeof(Token) *
            (size_t)lexer->pending_count
        );
    }

    int pending_count =
        lexer->pending_count;


    Token token = lexer_next_token(lexer);


    /*
     * Restaurar estado.
     */
    lexer->position = position;
    lexer->line = line;
    lexer->column = column;
    lexer->indent_top = indent_top;
    lexer->fstring_mode = fstring_mode;
    lexer->paren_depth = paren_depth;

    memcpy(
        lexer->indent_stack,
        indent_stack,
        sizeof(indent_stack)
    );


    /*
     * Liberamos los tokens pendientes creados
     * durante el peek.
     */
    for (int i = 0;
         i < lexer->pending_count;
         i++) {

        token_destroy(
            &lexer->pending_tokens[i]
        );
    }

    if (pending_count > 0 &&
        pending_backup != NULL) {

        memcpy(
            lexer->pending_tokens,
            pending_backup,
            sizeof(Token) *
            (size_t)pending_count
        );
    }

    lexer->pending_count =
        pending_count;

    free(pending_backup);

    return token;
}


/* =========================================================
 * ESTADO
 * ========================================================= */

int lexer_at_end(
    const Lexer *lexer
)
{
    if (lexer == NULL)
        return 1;

    return lexer_is_at_end(lexer);
}


void lexer_reset(
    Lexer *lexer
)
{
    if (lexer == NULL)
        return;

    /*
     * Liberar tokens pendientes.
     */
    for (int i = 0;
         i < lexer->pending_count;
         i++) {

        token_destroy(
            &lexer->pending_tokens[i]
        );
    }

    lexer->pending_count = 0;

    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;

    lexer->indent_top = 0;

    for (int i = 0;
         i < LEXER_MAX_INDENT_DEPTH;
         i++) {

        lexer->indent_stack[i] = 0;
    }

    lexer->fstring_mode = 0;
    lexer->paren_depth = 0;
}


int lexer_get_line(
    const Lexer *lexer
)
{
    if (lexer == NULL)
        return 0;

    return lexer->line;
}


int lexer_get_column(
    const Lexer *lexer
)
{
    if (lexer == NULL)
        return 0;

    return lexer->column;
}