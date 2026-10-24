#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"


/* =========================================================
 * PARSER
 * ========================================================= */

typedef struct Parser {

    /* Lexer que proporciona los tokens */
    Lexer *lexer;

    /* Token actual */
    Token current;

    /* Token siguiente */
    Token peek;

    /* Estado de errores */
    int had_error;

    /* El lexer ya reporto un error (p. ej. E1011) */
    int lex_error;

} Parser;


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

/*
 * Crea un parser usando un lexer existente.
 */
Parser *parser_create(Lexer *lexer);


/*
 * Libera el parser.
 */
void parser_destroy(Parser *parser);


/* =========================================================
 * PARSING
 * ========================================================= */

/*
 * Analiza todo el programa.
 *
 * Devuelve el AST raíz:
 *
 *     AST_PROGRAM
 */
ASTNode *parser_parse(Parser *parser);


/*
 * Analiza una declaración.
 */
ASTNode *parser_parse_declaration(Parser *parser);


/*
 * Analiza una sentencia.
 */
ASTNode *parser_parse_statement(Parser *parser);


/*
 * Analiza una expresión.
 */
ASTNode *parser_parse_expression(Parser *parser);


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

/*
 * var nombre:tipo = valor
 */
ASTNode *parser_parse_var(Parser *parser);


/*
 * const nombre:tipo = valor
 */
ASTNode *parser_parse_const(Parser *parser);


/*
 * fn nombre(...) -> tipo:
 */
ASTNode *parser_parse_function(Parser *parser);


/*
 * external "ruta.c" fn nombre(...) -> tipo
 *
 * Declaración de función externa (sin cuerpo).
 */
ASTNode *parser_parse_external(Parser *parser);


/*
 * struct Nombre:
 *     var campo:tipo
 */
ASTNode *parser_parse_struct(Parser *parser);


/*
 * import modulo
 *
 * import modulo as alias
 */
ASTNode *parser_parse_import(Parser *parser);


/*
 * from modulo import nombre
 */
ASTNode *parser_parse_from_import(Parser *parser);


/* =========================================================
 * SENTENCIAS
 * ========================================================= */

/*
 * if condicion:
 *     ...
 *
 * else:
 *     ...
 */
ASTNode *parser_parse_if(Parser *parser);


/*
 * while condicion:
 *     ...
 */
ASTNode *parser_parse_while(Parser *parser);


/*
 * for nombre in iterable:
 *     ...
 */
ASTNode *parser_parse_for(Parser *parser);


/*
 * return expresion
 */
ASTNode *parser_parse_return(Parser *parser);


/*
 * try:
 *     ...
 *
 * catch error:
 *     ...
 */
ASTNode *parser_parse_try(Parser *parser);


/*
 * break
 */
ASTNode *parser_parse_break(Parser *parser);


/*
 * continue
 */
ASTNode *parser_parse_continue(Parser *parser);


/*
 * pass
 */
ASTNode *parser_parse_pass(Parser *parser);


/*
 * Bloque basado en indentación.
 */
ASTNode *parser_parse_block(Parser *parser);


/* =========================================================
 * EXPRESIONES
 * ========================================================= */

/*
 * Expresiones de asignación.
 *
 * =
 * +=
 * -=
 * *=
 * /=
 */
ASTNode *parser_parse_assignment(Parser *parser);


/*
 * Operadores lógicos OR.
 */
ASTNode *parser_parse_or(Parser *parser);


/*
 * Operadores lógicos AND.
 */
ASTNode *parser_parse_and(Parser *parser);


/*
 * Comparaciones.
 */
ASTNode *parser_parse_comparison(Parser *parser);


/*
 * Suma y resta.
 */
ASTNode *parser_parse_term(Parser *parser);


/*
 * Multiplicación, división y módulo.
 */
ASTNode *parser_parse_factor(Parser *parser);


/*
 * Operadores unarios.
 *
 * -
 * not
 */
ASTNode *parser_parse_unary(Parser *parser);


/*
 * Expresión primaria.
 */
ASTNode *parser_parse_primary(Parser *parser);


/* =========================================================
 * TIPOS
 * ========================================================= */

/*
 * Analiza un tipo:
 *
 * int
 * float
 * string
 * bool
 * char
 * list<int>
 * map<string, int>
 */
Type *parser_parse_type(Parser *parser);


/* =========================================================
 * UTILIDADES
 * ========================================================= */

/*
 * Avanza al siguiente token.
 */
void parser_advance(Parser *parser);


/*
 * Comprueba si el token actual es de un tipo concreto.
 */
int parser_check(
    Parser *parser,
    TokenType type
);


/*
 * Consume un token concreto.
 *
 * Si no coincide, genera un error.
 */
int parser_expect(
    Parser *parser,
    TokenType type
);


/*
 * Devuelve 1 si el parser tiene errores.
 */
int parser_has_error(Parser *parser);


/*
 * Sincroniza el parser después de un error
 * para intentar continuar analizando el programa.
 */
void parser_synchronize(Parser *parser);

#endif