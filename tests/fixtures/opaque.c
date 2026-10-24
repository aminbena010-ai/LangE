/*
 * Fichero de prueba para handles opacos (Fase 2).
 *
 * Simula un "recurso" con fopen/fclose usando la stdio
 * real, devolviendo y aceptando void*.
 */

#include <stdio.h>
#include <stdint.h>

void *abrir_archivo(const char *ruta, const char *modo)
{
    return fopen(ruta, modo);
}

int64_t cerrar_archivo(void *recurso)
{
    if (recurso == NULL)
        return -1;

    return fclose((FILE *) recurso) == 0 ? 0 : -1;
}

int64_t es_nulo(void *recurso)
{
    return recurso == NULL ? 1 : 0;
}
