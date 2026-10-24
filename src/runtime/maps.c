/*
 * Runtime de E++ — map<K, V>.
 *
 * Separado de src/runtime.c en la fase C (ver error.log).
 * Sin dependencias del compilador.
 */

#include "runtime.h"
#include "runtime_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * map<K, V>
 *
 * Las entradas son contiguas y nunca se eliminan (el
 * lenguaje no define borrar claves), así que la posición
 * i-ésima es siempre válida para 0..length-1.
 * ======================================================= */

#define RIVO_MAP_INITIAL_CAPACITY 8

RivoMap *rivo_map_new(int64_t key_tag, int64_t value_tag)
{
    struct RivoMap *map = malloc(sizeof(struct RivoMap));

    if (map == NULL)
        rivo_fatal("MemoryError", "memoria insuficiente al crear un map");

    map->key_tag = key_tag;
    map->value_tag = value_tag;
    map->length = 0;
    map->capacity = RIVO_MAP_INITIAL_CAPACITY;
    map->entries = malloc(
        sizeof(struct RivoMapEntry) * RIVO_MAP_INITIAL_CAPACITY
    );

    if (map->entries == NULL)
        rivo_fatal("MemoryError", "memoria insuficiente al crear un map");

    rivo_heap_register(map, RIVO_HEAP_MAP);

    return map;
}


/*
 * Devuelve la posición de la clave o -1 si no existe.
 */
static int64_t rivo_map_find(
    struct RivoMap *map,
    int64_t key
)
{
    for (int64_t i = 0; i < map->length; i++) {

        if (rivo_value_equal(map->key_tag, map->entries[i].key, key))
            return i;
    }

    return -1;
}


void rivo_map_set(RivoMap *map, int64_t key, int64_t value)
{
    if (map == NULL)
        rivo_fatal("TypeError", "se intento usar un map nulo");

    int64_t position = rivo_map_find(map, key);

    if (position >= 0) {
        map->entries[position].value = value;
        return;
    }

    if (map->length == map->capacity) {

        int64_t new_capacity =
            map->capacity > 0 ? map->capacity * 2 : 8;

        struct RivoMapEntry *grown = realloc(
            map->entries,
            sizeof(struct RivoMapEntry) * (size_t) new_capacity
        );

        if (grown == NULL)
            rivo_fatal("MemoryError", "memoria insuficiente al ampliar un map");

        map->entries = grown;
        map->capacity = new_capacity;
    }

    map->entries[map->length].key = key;
    map->entries[map->length].value = value;
    map->length++;
}


int64_t rivo_map_get(RivoMap *map, int64_t key, int64_t line)
{
    if (map == NULL)
        rivo_fatal("TypeError",
            "se intento leer un map nulo en la linea %lld",
            (long long) line
        );

    int64_t position = rivo_map_find(map, key);

    if (position < 0) {

        if (map->key_tag == RIVO_TAG_STRING) {

            const char *text =
                (const char *) (intptr_t) key;

            rivo_fatal("KeyError",
                "la clave \"%s\" no existe en el map "
                "en la linea %lld",
                text != NULL ? text : "",
                (long long) line
            );
        }

        rivo_fatal("KeyError",
            "la clave no existe en el map en la linea %lld",
            (long long) line
        );
    }

    return map->entries[position].value;
}


int64_t rivo_map_len(RivoMap *map)
{
    return map != NULL ? map->length : 0;
}


void rivo_map_remove(RivoMap *map, int64_t key)
{
    if (map == NULL)
        rivo_fatal("TypeError", "se intento usar un map nulo");

    int64_t position = rivo_map_find(map, key);

    /* Clave inexistente: sin efecto (como lista.remove). */
    if (position < 0)
        return;

    for (int64_t i = position; i + 1 < map->length; i++)
        map->entries[i] = map->entries[i + 1];

    map->length--;
}


int64_t rivo_map_key_at(RivoMap *map, int64_t position, int64_t line)
{
    if (map == NULL)
        rivo_fatal("TypeError",
            "se intento recorrer un map nulo en la linea %lld",
            (long long) line
        );

    if (position < 0 || position >= map->length) {

        rivo_fatal("RuntimeError",
            "posicion %lld invalida al recorrer un map "
            "en la linea %lld (longitud %lld)",
            (long long) position,
            (long long) line,
            (long long) map->length
        );
    }

    return map->entries[position].key;
}
