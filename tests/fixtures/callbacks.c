/*
 * Fichero de prueba para callbacks FFI (Fase 2).
 *
 * `sum_squares` recibe un puntero a función C y lo invoca
 * con valores crecientes, acumulando el resultado.
 */

#include <stdint.h>

int64_t sum_squares(int64_t n, int64_t (*f)(int64_t))
{
    int64_t total = 0;

    for (int64_t i = 0; i < n; i++)
        total += f(i);

    return total;
}

/* Callback con dos parámetros: máximo de dos enteros. */
int64_t max2(int64_t a, int64_t b, int64_t (*cmp)(int64_t, int64_t))
{
    return cmp(a, b) >= 0 ? a : b;
}
