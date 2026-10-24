#ifndef TOKEN_H
#define TOKEN_H

/* =========================================================
 * TIPOS DE TOKEN
 * ========================================================= */

typedef enum {

    /* Fin / errores */
    TOKEN_EOF,
    TOKEN_ERROR,

    /* Identificadores y literales */
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    /* f-string: f"texto {expr} texto" */
    TOKEN_FSTRING_START,
    TOKEN_FSTRING_TEXT,
    TOKEN_FSTRING_END,

    /* Variables y constantes */
    TOKEN_VAR,
    TOKEN_CONST,

    /* Funciones */
    TOKEN_FN,
    TOKEN_RETURN,

    /* Estructuras */
    TOKEN_STRUCT,

    /* Concurrencia */
    TOKEN_ASYNC,
    TOKEN_AWAIT,

    /* Condicionales */
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,

    /* Bucles */
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,

    /* Sentencia vacía */
    TOKEN_PASS,

    /* Manejo de errores */
    TOKEN_TRY,
    TOKEN_CATCH,

    /* Imports */
    TOKEN_IMPORT,
    TOKEN_FROM,
    TOKEN_AS,
    TOKEN_EXTERNAL,

    /* Valores especiales */
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    /* Tipos */
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_BOOL,
    TOKEN_CHAR,
    TOKEN_LIST,
    TOKEN_MAP,
    TOKEN_TASK,

    /* Tipos FFI (solo en firmas externas) */
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_SIZE,
    TOKEN_VOID,

    /* Tipos FFI fase 2 */
    TOKEN_OPAQUE,
    TOKEN_CALLBACK,

    /* Tipos FFI fase 3 */
    TOKEN_BUFFER,

    /* Modificadores de retorno string en external */
    TOKEN_OWNED,
    TOKEN_STATIC,

    /* Operadores aritméticos */
    TOKEN_PLUS,          /* + */
    TOKEN_MINUS,         /* - */
    TOKEN_STAR,          /* * */
    TOKEN_SLASH,         /* / */
    TOKEN_FLOOR_DIV,     /* // */
    TOKEN_PERCENT,       /* % */
    TOKEN_INCREMENT,     /* ++ */
    TOKEN_DECREMENT,     /* -- */

    /* Operadores de asignación */
    TOKEN_ASSIGN,        /* = */
    TOKEN_PLUS_ASSIGN,   /* += */
    TOKEN_MINUS_ASSIGN,  /* -= */
    TOKEN_STAR_ASSIGN,   /* *= */
    TOKEN_SLASH_ASSIGN,  /* /= */

    /* Operadores de comparación */
    TOKEN_EQUAL,         /* == */
    TOKEN_NOT_EQUAL,     /* != */
    TOKEN_GREATER,       /* > */
    TOKEN_LESS,          /* < */
    TOKEN_GREATER_EQUAL, /* >= */
    TOKEN_LESS_EQUAL,    /* <= */

    /* Operadores lógicos */
    TOKEN_AND,           /* and */
    TOKEN_OR,            /* or */
    TOKEN_NOT,            /* not */

    /* Símbolos */
    TOKEN_COLON,         /* : */
    TOKEN_COMMA,         /* , */
    TOKEN_DOT,           /* . */
    TOKEN_ARROW,         /* -> */

    TOKEN_LPAREN,        /* ( */
    TOKEN_RPAREN,        /* ) */

    TOKEN_LBRACKET,      /* [ */
    TOKEN_RBRACKET,      /* ] */

    /*
     * Llaves: literales de mapa e interpolaciones de
     * f-strings. No delimitan bloques.
     */
    TOKEN_LBRACE,        /* { */
    TOKEN_RBRACE,        /* } */

    /* Indentación */
    TOKEN_INDENT,
    TOKEN_DEDENT,
    TOKEN_NEWLINE

} TokenType;


/* =========================================================
 * TOKEN
 * ========================================================= */

typedef struct Token {

    /* Tipo */
    TokenType type;

    /* Texto original del token */
    char *lexeme;

    /* Posición en el código fuente */
    int line;
    int column;

    /* Longitud del lexema */
    int length;

} Token;


/* =========================================================
 * FUNCIONES AUXILIARES
 * ========================================================= */

/*
 * Convierte un TokenType en texto.
 *
 * Ejemplo:
 *
 * TOKEN_INT -> "TOKEN_INT"
 */
const char *token_type_name(TokenType type);


/*
 * Libera la memoria utilizada por un token.
 */
void token_destroy(Token *token);

#endif