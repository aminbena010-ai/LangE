/*
 * Fichero de prueba para propiedad de strings (Fase 2).
 *
 * - `saludo_estatico` devuelve un literal (borrowed/static).
 * - `saludo_dinamico` devuelve un malloc (para `owned`).
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *saludo_estatico(void)
{
    return "hola estatico";
}

const char *saludo_estatico2(void)
{
    return "hola estatico";
}

char *saludo_dinamico(const char *nombre)
{
    size_t length = strlen(nombre) + 16;
    char *buffer = malloc(length);

    if (buffer == NULL)
        return NULL;

    snprintf(buffer, length, "hola %s", nombre);

    return buffer;
}
