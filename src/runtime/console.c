/*
 * Runtime de E++ — consola.
 *
 * Separado de src/runtime.c en la fase C (ver error.log).
 * Sin dependencias del compilador.
 */

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#endif

#include "runtime.h"
#include "runtime_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif


/* =========================================================
 * Consola
 * ======================================================= */

void rivo_clear(void)
{
    fputs("\033[2J\033[H", stdout);

    fflush(stdout);
}


void rivo_move(int64_t row, int64_t column)
{
    if (row < 1)
        row = 1;

    if (column < 1)
        column = 1;

    printf(
        "\033[%lld;%lldH",
        (long long) row,
        (long long) column
    );

    fflush(stdout);
}


char rivo_read_key(void)
{
#ifdef _WIN32
    if (_isatty(_fileno(stdin)))
        return (char) _getch();

    return (char) getchar();
#else
    unsigned char value = 0;

    if (isatty(STDIN_FILENO)) {

        struct termios original;
        struct termios raw;

        if (tcgetattr(STDIN_FILENO, &original) == 0) {

            raw = original;

            /* Una tecla, sin eco, sin esperar Intro. */
            raw.c_lflag &=
                (tcflag_t) ~(ICANON | ECHO);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;

            tcsetattr(STDIN_FILENO, TCSANOW, &raw);

            ssize_t read_count =
                read(STDIN_FILENO, &value, 1);

            tcsetattr(STDIN_FILENO, TCSANOW, &original);

            if (read_count != 1)
                return 0;

            return (char) value;
        #endif
        }
    }

    /* Tubería o terminal sin termios: lectura normal. */
    ssize_t read_count =
        read(STDIN_FILENO, &value, 1);

    if (read_count != 1)
        return 0;

    return (char) value;
}
