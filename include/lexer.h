#ifndef LEXER_H
#define LEXER_H

#include "token.h"


/* =========================================================
 * LEXER
 * ========================================================= */

typedef struct Lexer {

    /* Código fuente */
    const char *source;

    /*
     * Nombre del fichero (no se libera: lo posee quien
     * creó el lexer). Se usa en los diagnosticos.
     */
    const char *filename;

    /* Posición actual */
    int position;

    /* Línea y columna actuales */
    int line;
    int column;

    /* Longitud del código */
    int length;

    /* Indentación */
    int indent_stack[256];
    int indent_top;

    /*
     * Estado de un f-string en curso:
     *
     * 0 = fuera de un f-string
     * 1 = texto del f-string
     * 2 = expresión entre llaves {expr}
     */
    int fstring_mode;

    /*
     * Profundidad de paréntesis "(" ")" sin cerrar.
     *
     * Mientras es mayor que cero los saltos de línea no
     * producen TOKEN_NEWLINE y no se procesa la
     * indentación de la línea siguiente: una expresión o
     * una llamada puede partirse en varias líneas.
     */
    int paren_depth;

    /* Tokens pendientes de generar */
    Token *pending_tokens;

    int pending_count;
    int pending_capacity;

} Lexer;


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

Lexer *lexer_create(const char *source);

void lexer_destroy(Lexer *lexer);

/*
 * Asocia el nombre del fichero a los diagnosticos del
 * lexer (SyntaxError: ... \n --> fichero:linea:columna).
 */
void lexer_set_filename(
    Lexer *lexer,
    const char *filename
);


/* =========================================================
 * LECTURA DE TOKENS
 * ========================================================= */

/*
 * Obtiene el siguiente token del código fuente.
 */
Token lexer_next_token(Lexer *lexer);


/*
 * Mira el siguiente token sin consumirlo.
 */
Token lexer_peek_token(Lexer *lexer);


/* =========================================================
 * ESTADO
 * ========================================================= */

int lexer_at_end(const Lexer *lexer);


/*
 * Reinicia el lexer desde el principio.
 */
void lexer_reset(Lexer *lexer);


/* =========================================================
 * UTILIDADES
 * ========================================================= */

/*
 * Obtiene la línea actual.
 */
int lexer_get_line(const Lexer *lexer);


/*
 * Obtiene la columna actual.
 */
int lexer_get_column(const Lexer *lexer);

#endif