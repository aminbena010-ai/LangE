/*
 * Runtime de E++ — ficheros.
 *
 * Separado de src/runtime.c en la fase C (ver error.log).
 * Sin dependencias del compilador.
 */

#include "runtime.h"
#include "runtime_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* =========================================================
 * Ficheros
 * ======================================================= */

/* Tope de lectura: 64 MB. */
#define RIVO_MAX_FILE_BYTES (64 * 1024 * 1024)


char *rivo_read_file(const char *path)
{
    if (path == NULL)
        path = "";

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        rivo_fatal("FileNotFoundError", "no se pudo leer el archivo \"%s\"", path);

    size_t capacity = 4096;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (buffer == NULL) {

        fclose(file);
        rivo_fatal("MemoryError",
            "memoria insuficiente al leer \"%s\"",
            path
        );
    }

    while (1) {

        if (length == capacity) {

            if (capacity >= RIVO_MAX_FILE_BYTES) {

                free(buffer);
                fclose(file);

                rivo_fatal("RuntimeError",
                    "el archivo \"%s\" supera el limite "
                    "de lectura (64 MB)",
                    path
                );
            }

            size_t grown_capacity = capacity * 2;
            char *grown = realloc(buffer, grown_capacity);

            if (grown == NULL) {

                free(buffer);
                fclose(file);

                rivo_fatal("MemoryError",
                    "memoria insuficiente al leer \"%s\"",
                    path
                );
            }

            buffer = grown;
            capacity = grown_capacity;
        }

        size_t chunk = fread(
            buffer + length,
            1,
            capacity - length,
            file
        );

        length += chunk;

        if (chunk == 0)
            break;
    }

    if (ferror(file)) {

        free(buffer);
        fclose(file);

        rivo_fatal("FileNotFoundError", "no se pudo leer el archivo \"%s\"", path);
    }

    fclose(file);

    char *text = realloc(buffer, length + 1);

    if (text == NULL)
        text = buffer;

    text[length] = '\0';

    rivo_arena_register(text);

    return text;
}


uint8_t rivo_write_file(const char *path, const char *content)
{
    if (path == NULL)
        path = "";

    if (content == NULL)
        content = "";

    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;

    size_t length = strlen(content);

    size_t written = 0;

    if (length > 0)
        written = fwrite(content, 1, length, file);

    int closed = fclose(file) == 0;

    return (written == length && closed) ? 1 : 0;
}


uint8_t rivo_file_exists(const char *path)
{
    if (path == NULL)
        return 0;

#ifdef _WIN32
    struct _stat info;

    if (_stat(path, &info) != 0)
        return 0;

    return (info.st_mode & _S_IFMT) == _S_IFREG ? 1 : 0;
#else
    struct stat info;

    if (stat(path, &info) != 0)
        return 0;

    return S_ISREG(info.st_mode) ? 1 : 0;
#endif
}
