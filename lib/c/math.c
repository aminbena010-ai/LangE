/*
 * Biblioteca estandar de E++: matematicas.
 *
 * La declara lib/math.rivo con `external`, asi que
 * los nombres de estas funciones son exactamente los que
 * el programa en E++ llama. El compilador la compila con
 * los demas ficheros C externos y la enlaza con -lm.
 */

#include <math.h>

double square_root(double x)
{
    return sqrt(x);
}

double power(double x, double exponente)
{
    return pow(x, exponente);
}

double sine(double x)
{
    return sin(x);
}

double cosine(double x)
{
    return cos(x);
}

double tangent(double x)
{
    return tan(x);
}

double ln(double x)
{
    return log(x);
}

double log_10(double x)
{
    return log10(x);
}

double exponential(double x)
{
    return exp(x);
}

double pi(void)
{
    return 3.14159265358979323846;
}

double euler(void)
{
    return 2.71828182845904523536;
}

long long round_to_int(double x)
{
    return (long long) round(x);
}

long long floor_to_int(double x)
{
    return (long long) floor(x);
}

long long ceil_to_int(double x)
{
    return (long long) ceil(x);
}

unsigned char is_integer(double x)
{
    return x == floor(x) ? 1 : 0;
}
