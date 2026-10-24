#include "formatter.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FORMATTER_TAB_WIDTH 4


static char *formatter_read_file(
    const char *path,
    size_t *length
)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);

    fclose(file);

    buffer[read] = '\0';

    if (length != NULL)
        *length = read;

    return buffer;
}


static int formatter_write_file(
    const char *path,
    const char *text
)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;

    size_t length = strlen(text);
    int ok = fwrite(text, 1, length, file) == length;

    if (fclose(file) != 0)
        ok = 0;

    return ok;
}


/*
 * Normaliza una línea: expande tabuladores de la indentación y
 * elimina espacios finales. El contenido (incluidas cadenas y
 * comentarios) queda intacto.
 */
static void formatter_line(
    const char *input,
    size_t length,
    char *output,
    size_t capacity
)
{
    size_t out = 0;
    size_t i = 0;

    while (i < length &&
           (input[i] == ' ' || input[i] == '\t')) {

        if (input[i] == '\t') {
            size_t spaces =
                FORMATTER_TAB_WIDTH -
                (out % FORMATTER_TAB_WIDTH);

            while (spaces-- > 0 && out + 1 < capacity)
                output[out++] = ' ';
        } else if (out + 1 < capacity) {
            output[out++] = ' ';
        }

        i++;
    }

    size_t content_start = out;

    while (i < length) {
        if (out + 1 < capacity)
            output[out++] = input[i];
        i++;
    }

    while (out > content_start &&
           (output[out - 1] == ' ' ||
            output[out - 1] == '\t')) {
        out--;
    }

    output[out] = '\0';
}


char *formatter_format_text(const char *input)
{
    if (input == NULL)
        return NULL;

    size_t length = strlen(input);
    size_t capacity = length * 5 + 8;

    char *output = malloc(capacity);

    if (output == NULL)
        return NULL;

    size_t out = 0;
    int blank_lines = 0;
    int first_line = 1;

    const char *cursor = input;

    while (*cursor != '\0') {

        const char *end = strchr(cursor, '\n');
        size_t line_length = end != NULL
            ? (size_t)(end - cursor)
            : strlen(cursor);

        if (line_length > 0 &&
            cursor[line_length - 1] == '\r') {
            line_length--;
        }

        size_t line_capacity = line_length * 4 + 1;
        char *line = malloc(line_capacity);

        if (line == NULL) {
            free(output);
            return NULL;
        }

        formatter_line(
            cursor,
            line_length,
            line,
            line_capacity
        );

        if (line[0] == '\0') {
            blank_lines++;
        } else {
            if (!first_line && blank_lines > 0)
                output[out++] = '\n';

            blank_lines = 0;

            size_t line_out = strlen(line);

            memcpy(output + out, line, line_out);
            out += line_out;
            output[out++] = '\n';

            first_line = 0;
        }

        free(line);

        if (end == NULL)
            break;

        cursor = end + 1;
    }

    while (out > 0 && output[out - 1] == '\n')
        out--;

    if (out > 0)
        output[out++] = '\n';

    output[out] = '\0';

    return output;
}


static int formatter_process_file(
    const char *path,
    int check
)
{
    size_t length = 0;
    char *source = formatter_read_file(path, &length);

    if (source == NULL) {
        fprintf(stderr, "rivo fmt: no se pudo leer %s\n", path);
        return -1;
    }

    char *formatted = formatter_format_text(source);

    if (formatted == NULL) {
        free(source);
        return -1;
    }

    int changed = strcmp(formatted, source) != 0;

    free(source);

    if (!check && changed) {
        if (!formatter_write_file(path, formatted)) {
            fprintf(stderr, "rivo fmt: no se pudo escribir %s\n", path);
            free(formatted);
            return -1;
        }
    }

    free(formatted);

    if (check && changed) {
        fprintf(stderr, "rivo fmt: %s necesita formato\n", path);
        return 1;
    }

    return changed ? 1 : 0;
}


static int formatter_has_extension(const char *name)
{
    size_t length = strlen(name);

    return length >= 3 &&
           strcmp(name + length - 3, ".ro") == 0;
}


static int formatter_skip_dir(const char *name)
{
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, ".rivo") == 0 ||
           strcmp(name, "target") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "node_modules") == 0;
}


static int formatter_process_path(
    const char *path,
    int check,
    int *files,
    int *changes,
    int *errors
)
{
    struct stat info;

    if (stat(path, &info) != 0) {
        fprintf(stderr, "rivo fmt: %s: %s\n", path, strerror(errno));
        return 0;
    }

    if (S_ISREG(info.st_mode)) {

        if (!formatter_has_extension(path))
            return 0;

        (*files)++;

        int result = formatter_process_file(path, check);

        if (result < 0)
            (*errors)++;
        else if (result > 0)
            (*changes)++;

        return 1;
    }

    if (!S_ISDIR(info.st_mode))
        return 0;

    DIR *directory = opendir(path);

    if (directory == NULL) {
        fprintf(stderr, "rivo fmt: %s: %s\n", path, strerror(errno));
        return 0;
    }

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {

        if (formatter_skip_dir(entry->d_name))
            continue;

        size_t needed =
            strlen(path) + strlen(entry->d_name) + 2;

        char *child = malloc(needed);

        if (child == NULL) {
            (*errors)++;
            break;
        }

        snprintf(child, needed, "%s/%s", path, entry->d_name);

        formatter_process_path(
            child,
            check,
            files,
            changes,
            errors
        );

        free(child);
    }

    closedir(directory);

    return 1;
}


int formatter_run(
    int argc,
    char **argv
)
{
    int check = 0;
    const char *targets[256];
    int target_count = 0;

    /* argv[0] es el programa y argv[1] el subcomando "fmt". */
    for (int i = 2; i < argc; i++) {

        if (strcmp(argv[i], "--check") == 0) {
            check = 1;
            continue;
        }

        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0) {

            printf(
                "Uso: rivo fmt [--check] [archivo.ro | directorio ...]\n"
                "  --check   No escribe; falla si algo necesita formato.\n"
                "Sin argumentos formatea el directorio actual.\n"
            );

            return 0;
        }

        if (argv[i][0] == '-') {
            fprintf(stderr, "rivo fmt: opción desconocida %s\n", argv[i]);
            return 2;
        }

        if (target_count < (int)(sizeof(targets) / sizeof(targets[0])))
            targets[target_count++] = argv[i];
    }

    if (target_count == 0)
        targets[target_count++] = ".";

    int files = 0;
    int changes = 0;
    int errors = 0;

    for (int i = 0; i < target_count; i++) {
        formatter_process_path(
            targets[i],
            check,
            &files,
            &changes,
            &errors
        );
    }

    if (errors > 0)
        return 2;

    if (check && changes > 0)
        return 1;

    if (!check && files > 0)
        printf("rivo fmt: %d fichero(s), %d formateado(s)\n", files, changes);

    return 0;
}
