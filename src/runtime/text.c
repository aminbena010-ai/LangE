/*
 * Runtime de E++ — texto.
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


/* =========================================================
 * Texto
 *
 * Todas devuelven strings de la arena del runtime.
 * ======================================================= */

/*
 * Reserva un string en la arena: no se libera desde E++.
 */
static char *rivo_string_new(size_t size)
{
    char *text = malloc(size);

    if (text == NULL)
        rivo_fatal("MemoryError", "memoria insuficiente al crear un string");

    rivo_arena_register(text);

    return text;
}


static char *rivo_string_copy(const char *source)
{
    if (source == NULL)
        source = "";

    size_t length = strlen(source);
    char *text = rivo_string_new(length + 1);

    memcpy(text, source, length + 1);

    return text;
}


/*
 * Copia un C-string a la arena del runtime y devuelve el
 * puntero resultante (propiedad de E++). Para `-> string
 * owned` en FFI: el original queda libre para que C lo
 * libere con free().
 */
char *rivo_string_copy_arena(const char *source)
{
    return rivo_string_copy(source);
}


static int rivo_is_space(char character)
{
    return character == ' ' ||
           character == '\t' ||
           character == '\n' ||
           character == '\r' ||
           character == '\v' ||
           character == '\f';
}


RivoList *rivo_split(const char *text, const char *separator)
{
    if (text == NULL)
        text = "";

    if (separator == NULL)
        separator = "";

    RivoList *list = rivo_list_new(RIVO_TAG_STRING);

    size_t separator_length = strlen(separator);

    /* Separador vacío: un elemento por carácter. */
    if (separator_length == 0) {

        for (const char *cursor = text;
             *cursor != '\0';
             cursor++) {

            char *item = rivo_string_new(2);

            item[0] = *cursor;
            item[1] = '\0';

            rivo_list_add(
                list,
                (int64_t) (intptr_t) item
            );
        }

        return list;
    }

    const char *cursor = text;
    const char *hit;

    while ((hit = strstr(cursor, separator)) != NULL) {

        size_t piece = (size_t) (hit - cursor);

        char *item = rivo_string_new(piece + 1);

        memcpy(item, cursor, piece);
        item[piece] = '\0';

        rivo_list_add(
            list,
            (int64_t) (intptr_t) item
        );

        cursor = hit + separator_length;
    }

    /* Último trozo (aunque esté vacío). */
    size_t piece = strlen(cursor);

    char *item = rivo_string_new(piece + 1);

    memcpy(item, cursor, piece);
    item[piece] = '\0';

    rivo_list_add(
        list,
        (int64_t) (intptr_t) item
    );

    return list;
}


char *rivo_join(RivoList *list, const char *separator)
{
    if (separator == NULL)
        separator = "";

    size_t separator_length = strlen(separator);

    int64_t count = rivo_list_len(list);

    if (count <= 0)
        return rivo_string_copy("");

    size_t total = 0;

    for (int64_t i = 0; i < count; i++) {

        const char *item =
            (const char *) (intptr_t) list->items[i];

        total += item != NULL ? strlen(item) : 0;
    }

    total += separator_length * (size_t) (count - 1);

    char *result = rivo_string_new(total + 1);
    char *output = result;

    for (int64_t i = 0; i < count; i++) {

        const char *item =
            (const char *) (intptr_t) list->items[i];

        if (item == NULL)
            item = "";

        if (i > 0) {

            memcpy(output, separator, separator_length);
            output += separator_length;
        }

        size_t piece = strlen(item);

        memcpy(output, item, piece);
        output += piece;
    }

    *output = '\0';

    return result;
}


char *rivo_upper(const char *text)
{
    if (text == NULL)
        text = "";

    size_t length = strlen(text);
    char *result = rivo_string_new(length + 1);

    for (size_t i = 0; i < length; i++) {

        unsigned char character =
            (unsigned char) text[i];

        /* Clasificación ASCII: solo a-z. */
        result[i] =
            (character >= 'a' && character <= 'z')
                ? (char) (character - 32)
                : (char) character;
    }

    result[length] = '\0';

    return result;
}


char *rivo_lower(const char *text)
{
    if (text == NULL)
        text = "";

    size_t length = strlen(text);
    char *result = rivo_string_new(length + 1);

    for (size_t i = 0; i < length; i++) {

        unsigned char character =
            (unsigned char) text[i];

        result[i] =
            (character >= 'A' && character <= 'Z')
                ? (char) (character + 32)
                : (char) character;
    }

    result[length] = '\0';

    return result;
}


char *rivo_trim(const char *text)
{
    if (text == NULL)
        text = "";

    const char *start = text;

    while (*start != '\0' && rivo_is_space(*start))
        start++;

    size_t length = strlen(start);

    while (length > 0 && rivo_is_space(start[length - 1]))
        length--;

    char *result = rivo_string_new(length + 1);

    memcpy(result, start, length);
    result[length] = '\0';

    return result;
}


char *rivo_substring(
    const char *text,
    int64_t start,
    int64_t length
)
{
    if (text == NULL)
        text = "";

    int64_t total = (int64_t) strlen(text);

    if (length < 0) {

        rivo_fatal("ValueError",
            "substring: la longitud %lld no puede ser negativa",
            (long long) length
        );
    }

    if (start < 0 || start > total) {

        rivo_fatal("ValueError",
            "substring: el inicio %lld esta fuera del "
            "texto (longitud %lld)",
            (long long) start,
            (long long) total
        );
    }

    int64_t available = total - start;

    int64_t count =
        length < available ? length : available;

    char *result = rivo_string_new((size_t) count + 1);

    memcpy(result, text + start, (size_t) count);
    result[count] = '\0';

    return result;
}


char *rivo_replace(
    const char *text,
    const char *old_text,
    const char *new_text
)
{
    if (text == NULL)
        text = "";

    if (old_text == NULL)
        old_text = "";

    if (new_text == NULL)
        new_text = "";

    /* Qué reemplazar vacío: copia sin cambios. */
    if (old_text[0] == '\0')
        return rivo_string_copy(text);

    size_t text_length = strlen(text);
    size_t old_length = strlen(old_text);
    size_t new_length = strlen(new_text);

    /* Contar ocurrencias. */
    size_t count = 0;
    const char *cursor = text;

    while ((cursor = strstr(cursor, old_text)) != NULL) {

        count++;
        cursor += old_length;
    }

    if (count == 0)
        return rivo_string_copy(text);

    if (new_length > old_length) {

        size_t extra = new_length - old_length;

        if (count > (SIZE_MAX - text_length) / extra)
            rivo_fatal("MemoryError", "memoria insuficiente en replace");
    }

    size_t total =
        text_length - count * old_length +
        count * new_length;

    char *result = rivo_string_new(total + 1);
    char *output = result;

    cursor = text;

    while (1) {

        const char *hit = strstr(cursor, old_text);

        if (hit == NULL)
            break;

        size_t piece = (size_t) (hit - cursor);

        memcpy(output, cursor, piece);
        output += piece;

        memcpy(output, new_text, new_length);
        output += new_length;

        cursor = hit + old_length;
    }

    /* Resto hasta el final. */
    size_t rest = strlen(cursor);

    memcpy(output, cursor, rest);
    output[rest] = '\0';

    return result;
}


int64_t rivo_find(const char *text, const char *part)
{
    if (text == NULL)
        text = "";

    if (part == NULL)
        part = "";

    const char *hit = strstr(text, part);

    return hit != NULL ? (int64_t) (hit - text) : -1;
}


int64_t rivo_ord(const char *text)
{
    if (text == NULL || text[0] == '\0')
        rivo_fatal("ValueError", "ord(): la cadena esta vacia");

    return (int64_t) (unsigned char) text[0];
}


char *rivo_chr(int64_t value)
{
    if (value < 1 || value > 255)
        rivo_fatal("ValueError",
            "chr(): el valor %lld esta fuera del rango 1-255",
            (long long) value);

    char *result = rivo_string_new(2);

    result[0] = (char) (unsigned char) value;
    result[1] = '\0';

    return result;
}


uint8_t rivo_contains(const char *text, const char *part)
{
    return rivo_find(text, part) >= 0 ? 1 : 0;
}


uint8_t rivo_starts_with(
    const char *text,
    const char *prefix
)
{
    if (text == NULL)
        text = "";

    if (prefix == NULL)
        prefix = "";

    return strncmp(text, prefix, strlen(prefix)) == 0
        ? 1 : 0;
}


uint8_t rivo_ends_with(
    const char *text,
    const char *suffix
)
{
    if (text == NULL)
        text = "";

    if (suffix == NULL)
        suffix = "";

    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);

    if (suffix_length > text_length)
        return 0;

    return strcmp(
        text + (text_length - suffix_length),
        suffix
    ) == 0 ? 1 : 0;
}
