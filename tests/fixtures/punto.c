/*
 * Fichero de prueba para structs POD en FFI (Fase 3.2).
 *
 * `Punto` tiene dos campos int (escalares FFI), así que
 * se pasa como puntero (void*) a C.
 */

#include <stdint.h>
#include <math.h>

typedef struct {
    int64_t x;
    int64_t y;
} RivoPunto;

double distancia(const void *p)
{
    const RivoPunto *punto = (const RivoPunto *) p;
    double dx = (double) punto->x;
    double dy = (double) punto->y;
    return sqrt(dx * dx + dy * dy);
}

int64_t suma_componentes(const void *p)
{
    const RivoPunto *punto = (const RivoPunto *) p;
    return punto->x + punto->y;
}
