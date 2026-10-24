#include "token.h"

#include <stdlib.h>


/* =========================================================
 * NOMBRE DEL TIPO DE TOKEN
 * ========================================================= */

const char *token_type_name(TokenType type)
{
    switch (type) {

        /* -------------------------------------------------
         * Fin / errores
         * ------------------------------------------------- */

        case TOKEN_EOF:
            return "EOF";

        case TOKEN_ERROR:
            return "ERROR";


        /* -------------------------------------------------
         * Identificadores y literales
         * ------------------------------------------------- */

        case TOKEN_IDENTIFIER:
            return "IDENTIFIER";

        case TOKEN_INT_LITERAL:
            return "INT_LITERAL";

        case TOKEN_FLOAT_LITERAL:
            return "FLOAT_LITERAL";

        case TOKEN_STRING_LITERAL:
            return "STRING_LITERAL";

        case TOKEN_CHAR_LITERAL:
            return "CHAR_LITERAL";

        case TOKEN_FSTRING_START:
            return "FSTRING_START";

        case TOKEN_FSTRING_TEXT:
            return "FSTRING_TEXT";

        case TOKEN_FSTRING_END:
            return "FSTRING_END";


        /* -------------------------------------------------
         * Variables y constantes
         * ------------------------------------------------- */

        case TOKEN_VAR:
            return "VAR";

        case TOKEN_CONST:
            return "CONST";


        /* -------------------------------------------------
         * Funciones
         * ------------------------------------------------- */

        case TOKEN_FN:
            return "FN";

        case TOKEN_RETURN:
            return "RETURN";

        case TOKEN_STRUCT:
            return "STRUCT";

        case TOKEN_ASYNC:
            return "ASYNC";

        case TOKEN_AWAIT:
            return "AWAIT";


        /* -------------------------------------------------
         * Condicionales
         * ------------------------------------------------- */

        case TOKEN_IF:
            return "IF";

        case TOKEN_ELIF:
            return "ELIF";

        case TOKEN_ELSE:
            return "ELSE";


        /* -------------------------------------------------
         * Bucles
         * ------------------------------------------------- */

        case TOKEN_WHILE:
            return "WHILE";

        case TOKEN_FOR:
            return "FOR";

        case TOKEN_IN:
            return "IN";

        case TOKEN_BREAK:
            return "BREAK";

        case TOKEN_CONTINUE:
            return "CONTINUE";


        /* -------------------------------------------------
         * Sentencia vacía
         * ------------------------------------------------- */

        case TOKEN_PASS:
            return "PASS";


        /* -------------------------------------------------
         * Manejo de errores
         * ------------------------------------------------- */

        case TOKEN_TRY:
            return "TRY";

        case TOKEN_CATCH:
            return "CATCH";


        /* -------------------------------------------------
         * Imports
         * ------------------------------------------------- */

        case TOKEN_IMPORT:
            return "IMPORT";

        case TOKEN_FROM:
            return "FROM";

        case TOKEN_AS:
            return "AS";

        case TOKEN_EXTERNAL:
            return "EXTERNAL";


        /* -------------------------------------------------
         * Valores especiales
         * ------------------------------------------------- */

        case TOKEN_TRUE:
            return "TRUE";

        case TOKEN_FALSE:
            return "FALSE";

        case TOKEN_NULL:
            return "NULL";


        /* -------------------------------------------------
         * Tipos
         * ------------------------------------------------- */

        case TOKEN_INT:
            return "INT";

        case TOKEN_FLOAT:
            return "FLOAT";

        case TOKEN_STRING:
            return "STRING";

        case TOKEN_BOOL:
            return "BOOL";

        case TOKEN_CHAR:
            return "CHAR";

        case TOKEN_LIST:
            return "LIST";

        case TOKEN_MAP:
            return "MAP";

        case TOKEN_TASK:
            return "TASK";

        case TOKEN_I8:
            return "I8";

        case TOKEN_I16:
            return "I16";

        case TOKEN_I32:
            return "I32";

        case TOKEN_U8:
            return "U8";

        case TOKEN_U16:
            return "U16";

        case TOKEN_U32:
            return "U32";

        case TOKEN_U64:
            return "U64";

        case TOKEN_SIZE:
            return "SIZE";

        case TOKEN_VOID:
            return "VOID";

        case TOKEN_OPAQUE:
            return "OPAQUE";

        case TOKEN_CALLBACK:
            return "CALLBACK";

        case TOKEN_BUFFER:
            return "BUFFER";

        case TOKEN_OWNED:
            return "OWNED";

        case TOKEN_STATIC:
            return "STATIC";


        /* -------------------------------------------------
         * Operadores aritméticos
         * ------------------------------------------------- */

        case TOKEN_PLUS:
            return "PLUS";

        case TOKEN_MINUS:
            return "MINUS";

        case TOKEN_STAR:
            return "STAR";

        case TOKEN_SLASH:
            return "SLASH";

        case TOKEN_FLOOR_DIV:
            return "FLOOR_DIV";

        case TOKEN_PERCENT:
            return "PERCENT";

        case TOKEN_INCREMENT:
            return "INCREMENT";

        case TOKEN_DECREMENT:
            return "DECREMENT";


        /* -------------------------------------------------
         * Operadores de asignación
         * ------------------------------------------------- */

        case TOKEN_ASSIGN:
            return "ASSIGN";

        case TOKEN_PLUS_ASSIGN:
            return "PLUS_ASSIGN";

        case TOKEN_MINUS_ASSIGN:
            return "MINUS_ASSIGN";

        case TOKEN_STAR_ASSIGN:
            return "STAR_ASSIGN";

        case TOKEN_SLASH_ASSIGN:
            return "SLASH_ASSIGN";


        /* -------------------------------------------------
         * Operadores de comparación
         * ------------------------------------------------- */

        case TOKEN_EQUAL:
            return "EQUAL";

        case TOKEN_NOT_EQUAL:
            return "NOT_EQUAL";

        case TOKEN_GREATER:
            return "GREATER";

        case TOKEN_LESS:
            return "LESS";

        case TOKEN_GREATER_EQUAL:
            return "GREATER_EQUAL";

        case TOKEN_LESS_EQUAL:
            return "LESS_EQUAL";


        /* -------------------------------------------------
         * Operadores lógicos
         * ------------------------------------------------- */

        case TOKEN_AND:
            return "AND";

        case TOKEN_OR:
            return "OR";

        case TOKEN_NOT:
            return "NOT";


        /* -------------------------------------------------
         * Símbolos
         * ------------------------------------------------- */

        case TOKEN_COLON:
            return "COLON";

        case TOKEN_COMMA:
            return "COMMA";

        case TOKEN_DOT:
            return "DOT";

        case TOKEN_ARROW:
            return "ARROW";


        /* -------------------------------------------------
         * Paréntesis
         * ------------------------------------------------- */

        case TOKEN_LPAREN:
            return "LPAREN";

        case TOKEN_RPAREN:
            return "RPAREN";


        /* -------------------------------------------------
         * Corchetes
         * ------------------------------------------------- */

        case TOKEN_LBRACKET:
            return "LBRACKET";

        case TOKEN_RBRACKET:
            return "RBRACKET";


        /* -------------------------------------------------
         * Llaves (solo dentro de f-strings)
         * ------------------------------------------------- */

        case TOKEN_LBRACE:
            return "LBRACE";

        case TOKEN_RBRACE:
            return "RBRACE";


        /* -------------------------------------------------
         * Indentación
         * ------------------------------------------------- */

        case TOKEN_INDENT:
            return "INDENT";

        case TOKEN_DEDENT:
            return "DEDENT";

        case TOKEN_NEWLINE:
            return "NEWLINE";


        /* -------------------------------------------------
         * Desconocido
         * ------------------------------------------------- */

        default:
            return "UNKNOWN";
    }
}


/* =========================================================
 * DESTRUCCIÓN DE TOKEN
 * ========================================================= */

void token_destroy(Token *token)
{
    if (token == NULL)
        return;

    free(token->lexeme);

    token->lexeme = NULL;
    token->length = 0;
    token->line = 0;
    token->column = 0;
    token->type = TOKEN_EOF;
}