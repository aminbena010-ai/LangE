/*
 * Runtime de E++ — list<T>.
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
 * list<T>
 * ======================================================= */

#define RIVO_LIST_INITIAL_CAPACITY 8

RivoList *rivo_list_new(int64_t element_tag)
{
    struct RivoList *list = malloc(sizeof(struct RivoList));

    if (list == NULL)
        rivo_fatal("MemoryError", "memoria insuficiente al crear una lista");

    list->element_tag = element_tag;
    list->length = 0;
    list->capacity = RIVO_LIST_INITIAL_CAPACITY;
    list->items = malloc(
        sizeof(int64_t) * RIVO_LIST_INITIAL_CAPACITY
    );

    if (list->items == NULL)
        rivo_fatal("MemoryError", "memoria insuficiente al crear una lista");

    rivo_heap_register(list, RIVO_HEAP_LIST);

    return list;
}


void rivo_list_add(RivoList *list, int64_t value)
{
    if (list == NULL)
        rivo_fatal("TypeError", "se intento usar una lista nula");

    if (list->length == list->capacity) {

        int64_t new_capacity =
            list->capacity > 0 ? list->capacity * 2 : 8;

        int64_t *grown = realloc(
            list->items,
            sizeof(int64_t) * (size_t) new_capacity
        );

        if (grown == NULL)
            rivo_fatal("MemoryError", "memoria insuficiente al ampliar una lista");

        list->items = grown;
        list->capacity = new_capacity;
    }

    list->items[list->length++] = value;
}


void rivo_list_remove(RivoList *list, int64_t value)
{
    if (list == NULL)
        rivo_fatal("TypeError", "se intento usar una lista nula");

    for (int64_t i = 0; i < list->length; i++) {

        if (!rivo_value_equal(
                list->element_tag,
                list->items[i],
                value
            ))
        {
            continue;
        }

        for (int64_t j = i; j + 1 < list->length; j++)
            list->items[j] = list->items[j + 1];

        list->length--;

        /* Solo la primera ocurrencia (claude.md §11). */
        return;
    }
}


int64_t rivo_list_len(RivoList *list)
{
    return list != NULL ? list->length : 0;
}


int64_t rivo_list_get(RivoList *list, int64_t index, int64_t line)
{
    if (list == NULL)
        rivo_fatal("TypeError",
            "se intento leer una lista nula en la linea %lld",
            (long long) line
        );

    if (index < 0 || index >= list->length) {

        rivo_fatal("IndexError",
            "indice %lld fuera de rango en la linea %lld "
            "(longitud %lld)",
            (long long) index,
            (long long) line,
            (long long) list->length
        );
    }

    return list->items[index];
}


void rivo_list_set(
    RivoList *list,
    int64_t index,
    int64_t value,
    int64_t line
)
{
    if (list == NULL)
        rivo_fatal("TypeError",
            "se intento escribir en una lista nula en la linea %lld",
            (long long) line
        );

    if (index < 0 || index >= list->length) {

        rivo_fatal("IndexError",
            "indice %lld fuera de rango en la linea %lld "
            "(longitud %lld)",
            (long long) index,
            (long long) line,
            (long long) list->length
        );
    }

    list->items[index] = value;
}


RivoList *rivo_concat_list(RivoList *left, RivoList *right)
{
    if (left == NULL)
        return right;

    if (right == NULL)
        return left;

    RivoList *result = rivo_list_new(left->element_tag);

    for (int64_t i = 0; i < left->length; i++)
        rivo_list_add(result, left->items[i]);

    for (int64_t i = 0; i < right->length; i++)
        rivo_list_add(result, right->items[i]);

    return result;
}


/* =========================================================
 * Builtins de lista (2026-10-10)
 * ======================================================= */

/* range(a, b) -> list<int>: [a, a+1, ..., b-1] */
RivoList *rivo_range(int64_t start, int64_t end)
{
    RivoList *result = rivo_list_new(RIVO_TAG_INT);

    for (int64_t i = start; i < end; i++)
        rivo_list_add(result, i);

    return result;
}


/* divmod(a, b) -> list<int>: [a/b, a%b] */
RivoList *rivo_divmod(int64_t a, int64_t b)
{
    if (b == 0)
        rivo_fatal("ZeroDivisionError", "divmod(): division por cero");

    RivoList *result = rivo_list_new(RIVO_TAG_INT);

    rivo_list_add(result, a / b);
    rivo_list_add(result, a % b);

    return result;
}


/*
 * sorted(list<int>) -> list<int>: copia ordenada
 * ascendente (insertion sort). La semántica solo admite
 * list<int>, por eso la comparación usa los enteros
 * crudos.
 */
RivoList *rivo_sorted(RivoList *list)
{
    if (list == NULL)
        rivo_fatal("TypeError", "sorted(): lista nula");

    RivoList *result = rivo_list_new(list->element_tag);

    for (int64_t i = 0; i < list->length; i++)
        rivo_list_add(result, list->items[i]);

    for (int64_t i = 1; i < result->length; i++) {

        int64_t key = result->items[i];
        int64_t j = i - 1;

        while (j >= 0 && result->items[j] > key) {
            result->items[j + 1] = result->items[j];
            j--;
        }

        result->items[j + 1] = key;
    }

    return result;
}


/* reversed(list) -> list: invierte el orden (cualquier tipo). */
RivoList *rivo_reversed(RivoList *list)
{
    if (list == NULL)
        rivo_fatal("TypeError", "reversed(): lista nula");

    RivoList *result = rivo_list_new(list->element_tag);

    for (int64_t i = list->length - 1; i >= 0; i--)
        rivo_list_add(result, list->items[i]);

    return result;
}


/* all(list<bool>) -> bool: ¿todos los elementos son ciertos? */
uint8_t rivo_all(RivoList *list)
{
    if (list == NULL)
        rivo_fatal("TypeError", "all(): lista nula");

    for (int64_t i = 0; i < list->length; i++)
        if (list->items[i] == 0)
            return 0;

    return 1;
}


/* any(list<bool>) -> bool: ¿algún elemento es cierto? */
uint8_t rivo_any(RivoList *list)
{
    if (list == NULL)
        rivo_fatal("TypeError", "any(): lista nula");

    for (int64_t i = 0; i < list->length; i++)
        if (list->items[i] != 0)
            return 1;

    return 0;
}
