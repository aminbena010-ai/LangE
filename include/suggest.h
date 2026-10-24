#ifndef SUGGEST_H
#define SUGGEST_H


/* =========================================================
 * SUGERENCIAS DE NOMBRES (sistema REM)
 *
 * Distancia de Levenshtein y búsqueda del candidato más
 * parecido: si el usuario escribe `prnt`, el sistema
 * sugiere `print`.
 * ========================================================= */


/*
 * Distancia de Levenshtein entre dos cadenas.
 * Devuelve -1 si supera el umbral interno (no interesa
 * comparar cadenas demasiado distintas).
 */
int suggest_distance(
    const char *word,
    const char *candidate
);


/*
 * Devuelve el candidato más parecido a `word` si su
 * distancia está dentro del umbral; NULL si no hay
 * ninguno razonable.
 *
 * El puntero devuelto pertenece a `candidates` (o es un
 * literal), no se copia nada.
 */
const char *suggest_best(
    const char *word,
    const char *const *candidates,
    int count
);


#endif /* SUGGEST_H */
