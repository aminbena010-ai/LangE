/* Biblioteca estandar: color (ANSI).
 *
 * Los strings que devuelve el runtime desde C deben ser
 * static: se usa un anillo de buffers. Los textos con mas
 * de 8191 bytes se recortan.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BUFFERS 8
#define COLOR_SIZE 8192

static char anillo[COLOR_BUFFERS][COLOR_SIZE];
static int siguiente = 0;

static char *new_buffer(void)
{
    char *b = anillo[siguiente];
    siguiente = (siguiente + 1) % COLOR_BUFFERS;
    return b;
}

/* Envuelve texto con un codigo ANSI y lo cierra con reset. */
const char *color_colorize(int64_t codigo, const char *texto)
{
    char *b = new_buffer();

    if (texto == NULL)
        texto = "";

    snprintf(
        b,
        COLOR_SIZE,
        "\033[%lldm%s\033[0m",
        (long long) codigo,
        texto
    );

    return b;
}

/*
 * Color de 256 (38;5;N en primer plano, 48;5;N en fondo).
 * N se recorta a 0-255.
 */
const char *color_colorize_256(
    int64_t codigo,
    int64_t plano,
    const char *texto
)
{
    char *b = new_buffer();

    if (texto == NULL)
        texto = "";

    if (codigo < 0)
        codigo = 0;

    if (codigo > 255)
        codigo = 255;

    snprintf(
        b,
        COLOR_SIZE,
        "\033[%lld;5;%lldm%s\033[0m",
        (long long) (plano ? 48 : 38),
        (long long) codigo,
        texto
    );

    return b;
}

/*
 * Color RGB verdadero (38;2;r;g;b en primer plano,
 * 48;2;r;g;b en fondo). Cada canal se recorta a 0-255.
 */
const char *color_colorize_rgb(
    int64_t r,
    int64_t g,
    int64_t b,
    int64_t plano,
    const char *texto
)
{
    char *out = new_buffer();

    if (texto == NULL)
        texto = "";

    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    snprintf(
        out,
        COLOR_SIZE,
        "\033[%lld;2;%lld;%lld;%lldm%s\033[0m",
        (long long) (plano ? 48 : 38),
        (long long) r,
        (long long) g,
        (long long) b,
        texto
    );

    return out;
}

/* Quita todas las secuencias ESC [ ... m del texto. */
const char *color_no_color(const char *texto)
{
    char *b = new_buffer();
    size_t n;
    size_t i = 0;
    size_t j = 0;

    if (texto == NULL)
        texto = "";

    n = strlen(texto);

    while (i < n) {

        if (texto[i] == '\033' && i + 1 < n && texto[i + 1] == '[') {
            i = i + 2;

            while (i < n && texto[i] != 'm')
                i = i + 1;

            if (i < n)
                i = i + 1;

            continue;
        }

        if (j + 1 < COLOR_SIZE)
            b[j] = texto[i];

        j = j + 1;
        i = i + 1;
    }

    b[j < COLOR_SIZE ? j : COLOR_SIZE - 1] = '\0';

    return b;
}
