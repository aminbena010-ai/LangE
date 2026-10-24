/*
 * Runtime de E++ — funciones incorporadas: aleatoriedad y tiempo.
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
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


/* =========================================================
 * Funciones incorporadas (fase 2.1)
 *
 * Firmas y semántica aprobadas: ver include/builtins.h y
 * include/runtime.h. Cada una lleva el prefijo `rivo_` y el
 * codegen la declara desde la tabla compartida.
 * ======================================================= */

/* =========================================================
 * Aleatoriedad y tiempo
 * ======================================================= */

/*
 * splitmix64: PRNG de un solo registro, determinista a
 * partir de la semilla, así que rivo_seed(42) siempre
 * devuelve la misma secuencia (útil para tests).
 */
static uint64_t rivo_random_state = 0;
static int rivo_random_seeded = 0;

static uint64_t rivo_random_next(void)
{
    if (!rivo_random_seeded) {

        struct timespec now;

        timespec_get(&now, TIME_UTC);

        rivo_random_state =
            (uint64_t) now.tv_sec * 1000000000ULL +
            (uint64_t) now.tv_nsec +
            (uint64_t) (uintptr_t) &rivo_random_state;

        rivo_random_seeded = 1;
    }

    rivo_random_state += 0x9E3779B97F4A7C15ULL;

    uint64_t value = rivo_random_state;

    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;

    return value ^ (value >> 31);
}


void rivo_seed(int64_t value)
{
    rivo_random_state = (uint64_t) value;
    rivo_random_seeded = 1;
}


int64_t rivo_random(int64_t minimum, int64_t maximum)
{
    if (maximum < minimum) {

        rivo_fatal("ValueError",
            "random: el minimo %lld es mayor que el maximo %lld",
            (long long) minimum,
            (long long) maximum
        );
    }

    /*
     * Se resta en sin signo: con signo, INT64_MIN - 1
     * desbordaría.
     */
    uint64_t span =
        (uint64_t) maximum - (uint64_t) minimum + 1;

    /* span == 0: cubre todos los int64_t. */
    if (span == 0)
        return (int64_t) rivo_random_next();

    return minimum +
           (int64_t) (rivo_random_next() % span);
}


double rivo_random_float(void)
{
    /* 53 bits aleatorios en la parte alta -> [0.0, 1.0). */
    return (double) (rivo_random_next() >> 11) *
           (1.0 / 9007199254740992.0);
}


int64_t rivo_time_ms(void)
{
#ifdef _WIN32
    static ULONGLONG start = 0;
    ULONGLONG now = GetTickCount64();

    if (start == 0) {
        start = now;
        return 0;
    }

    return (int64_t) (now - start);
#else
    static struct timespec start;
    static int started = 0;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!started) {

        start = now;
        started = 1;

        return 0;
    }

    int64_t seconds =
        (int64_t) now.tv_sec - (int64_t) start.tv_sec;

    int64_t nanoseconds =
        (int64_t) now.tv_nsec - (int64_t) start.tv_nsec;

    return seconds * 1000 + nanoseconds / 1000000;
#endif
}


int64_t rivo_epoch_ms(void)
{
    struct timespec now;

    timespec_get(&now, TIME_UTC);

    return (int64_t) now.tv_sec * 1000 +
           (int64_t) now.tv_nsec / 1000000;
}


void rivo_sleep_ms(int64_t milliseconds)
{
    if (milliseconds <= 0)
        return;

#ifdef _WIN32
    while (milliseconds > 0) {
        DWORD interval = milliseconds > MAXDWORD
            ? MAXDWORD
            : (DWORD) milliseconds;
        Sleep(interval);
        milliseconds -= interval;
    }
#else
    struct timespec requested;

    requested.tv_sec = (time_t) (milliseconds / 1000);
    requested.tv_nsec =
        (long) ((milliseconds % 1000) * 1000000);

    while (nanosleep(&requested, &requested) != 0)
        ;
#endif
}
