/*
 * Fichero de prueba para buffer<T> en FFI (Fase 3.3).
 *
 * `suma_buffer` recibe un puntero y una longitud, y
 * calcula la suma de los elementos.
 */

#include <stdint.h>

int64_t suma_buffer(const int64_t *buf, int64_t n)
{
    int64_t total = 0;

    for (int64_t i = 0; i < n; i++)
        total += buf[i];

    return total;
}

double media_buffer(const int64_t *buf, int64_t n)
{
    if (n == 0)
        return 0.0;

    int64_t total = 0;

    for (int64_t i = 0; i < n; i++)
        total += buf[i];

    return (double) total / (double) n;
}
