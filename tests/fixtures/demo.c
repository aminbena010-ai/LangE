/*
 * Fichero C de prueba para `external`.
 *
 * Mapeo de tipos E++ <-> C:
 *
 *     int       -> long long
 *     float     -> double
 *     string    -> const char *
 *     bool      -> unsigned char (0 / 1)
 *     char  -> char
 *
 * Las cadenas que devuelve tienen que ser estáticas: E++
 * no libera lo que devuelve C.
 */

#include <stdio.h>

long long sum(long long a, long long b)
{
    return a + b;
}

void saludo(const char *nombre)
{
    printf("Hola %s\n", nombre);
}

const char *etiqueta(const char *texto)
{
    static char buffer[128];

    snprintf(buffer, sizeof(buffer), "[%s]", texto);

    return buffer;
}

unsigned char es_positivo(long long valor)
{
    return valor > 0 ? 1 : 0;
}

/* Fuerza un residuo binario: 0.1 + 0.2 no es 0.3 en double,
   pero el programa E++ espera ver "0.3" (snap en la frontera
   FFI, claude.md §53). */
double suma_decimal(void)
{
    return 0.1 + 0.2;
}
