#ifndef BUILTINS_H
#define BUILTINS_H

#include "types.h"

#include <string.h>

/*
 * Funciones incorporadas de E++ (fase 2.1):
 * aleatoriedad, tiempo, texto, ficheros y consola.
 *
 * Una sola tabla para la semántica (número y tipo de
 * argumentos, resultado) y para el codegen (firma de la
 * función runtime), de forma que las dos fases no puedan
 * divergir. El codegen traduce cada entrada a la función
 * runtime `rivo_<nombre>` con el mismo nombre.
 *
 * Decisión de diseño 2026-10-06: firmas aprobadas por el
 * usuario (ver error.log).
 *
 * Notas de diseño:
 *   - TYPE_LIST como argumento es siempre list<string>
 *     (solo lo usa join) y como resultado es siempre
 *     list<string> (solo lo devuelve split).
 *   - TYPE_NULL como argumento marca posición sin usar y
 *     como resultado significa que no devuelve valor.
 */

typedef struct RivoBuiltin {
    const char *name;
    int argument_count;
    TypeKind arguments[3];
    TypeKind result;
} RivoBuiltin;

static const RivoBuiltin rivo_builtins[] = {

    /* ---- Aleatoriedad y tiempo ---- */

    { "random",
      2, { TYPE_INT, TYPE_INT, TYPE_NULL }, TYPE_INT },

    { "random_float",
      0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_FLOAT },

    { "seed",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_NULL },

    { "time_ms",
      0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    { "epoch_ms",
      0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    /* ---- Texto ---- */

    { "concat",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_STRING },

    { "split",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_LIST },

    { "join",
      2, { TYPE_LIST, TYPE_STRING, TYPE_NULL }, TYPE_STRING },

    { "upper",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "lower",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "trim",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "substring",
      3, { TYPE_STRING, TYPE_INT, TYPE_INT }, TYPE_STRING },

    { "replace",
      3, { TYPE_STRING, TYPE_STRING, TYPE_STRING },
      TYPE_STRING },

    { "contains",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_BOOL },

    { "starts_with",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_BOOL },

    { "ends_with",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_BOOL },

    { "find",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_INT },

    { "ord",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    { "chr",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    /* ---- Conversion y numericos ---- */

    { "to_int",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    { "to_float",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_FLOAT },

    { "bin",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "oct",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "hex",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "round",
      1, { TYPE_FLOAT, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    { "pow",
      2, { TYPE_INT, TYPE_INT, TYPE_NULL }, TYPE_INT },

    { "abs",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_INT },

    /* ---- Ficheros ---- */

    { "read_file",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_STRING },

    { "write_file",
      2, { TYPE_STRING, TYPE_STRING, TYPE_NULL }, TYPE_BOOL },

    { "file_exists",
      1, { TYPE_STRING, TYPE_NULL, TYPE_NULL }, TYPE_BOOL },

    /* ---- Consola ---- */

    { "clear",
      0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_NULL },

    { "move",
      2, { TYPE_INT, TYPE_INT, TYPE_NULL }, TYPE_NULL },

    { "sleep_ms",
      1, { TYPE_INT, TYPE_NULL, TYPE_NULL }, TYPE_NULL },

    { "read_key",
      0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_CHAR },

    { NULL, 0, { TYPE_NULL, TYPE_NULL, TYPE_NULL }, TYPE_NULL }
};


static const RivoBuiltin *rivo_builtin_find(const char *name)
{
    if (name == NULL)
        return NULL;

    for (int i = 0; rivo_builtins[i].name != NULL; i++) {

        if (strcmp(rivo_builtins[i].name, name) == 0)
            return &rivo_builtins[i];
    }

    return NULL;
}

#endif /* BUILTINS_H */
