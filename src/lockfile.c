/* =========================================================
 * LOCKFILE (rivo.lock): lectura y escritura.
 *
 * Formato:
 *     version = 1
 *
 *     [[paquete]]
 *     nombre = "saludo"
 *     version = "0.1.0"
 *     origen = "ruta:../libs/saludo"
 *     deps = []
 * ======================================================= */

#include "lockfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lock_duplicate(const char *text)
{
    if (text == NULL)
        return NULL;

    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL)
        memcpy(copy, text, length);

    return copy;
}


static char *lock_trim(char *text)
{
    if (text == NULL)
        return NULL;

    while (*text == ' ' || *text == '\t')
        text++;

    char *end = text + strlen(text);

    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n'))
        end--;

    *end = '\0';

    return text;
}


LockFile *lockfile_new(void)
{
    return calloc(1, sizeof(LockFile));
}


static void lockfile_free_package(LockedPackage *package)
{
    if (package == NULL)
        return;

    free(package->name);
    free(package->version);
    free(package->source);

    for (int i = 0; i < package->dep_count; i++)
        free(package->deps[i]);

    free(package->deps);
}


void lockfile_destroy(LockFile *lock)
{
    if (lock == NULL)
        return;

    for (int i = 0; i < lock->count; i++)
        lockfile_free_package(&lock->packages[i]);

    free(lock->packages);
    free(lock);
}


LockedPackage *lockfile_find(LockFile *lock, const char *name)
{
    if (lock == NULL || name == NULL)
        return NULL;

    for (int i = 0; i < lock->count; i++) {

        if (lock->packages[i].name != NULL &&
            strcmp(lock->packages[i].name, name) == 0)
            return &lock->packages[i];
    }

    return NULL;
}


int lockfile_add(
    LockFile *lock,
    const char *name,
    const char *version,
    const char *source,
    const char *const *deps,
    int dep_count
)
{
    if (lock == NULL || name == NULL)
        return 0;

    if (lockfile_find(lock, name) != NULL)
        return 0;

    LockedPackage *grown =
        realloc(
            lock->packages,
            (size_t)(lock->count + 1) *
                sizeof(LockedPackage)
        );

    if (grown == NULL)
        return 0;

    lock->packages = grown;

    LockedPackage *package = &lock->packages[lock->count];

    memset(package, 0, sizeof(*package));

    package->name = lock_duplicate(name);
    package->version =
        lock_duplicate(version ? version : "0.0.0");
    package->source =
        lock_duplicate(source ? source : "");

    if (package->name == NULL ||
        package->version == NULL ||
        package->source == NULL) {

        lockfile_free_package(package);
        return 0;
    }

    if (dep_count > 0) {

        package->deps =
            calloc(
                (size_t) dep_count,
                sizeof(char *)
            );

        if (package->deps == NULL) {

            lockfile_free_package(package);
            return 0;
        }

        for (int i = 0; i < dep_count; i++) {

            package->deps[i] =
                lock_duplicate(deps[i]);

            if (package->deps[i] == NULL) {

                lockfile_free_package(package);
                return 0;
            }

            package->dep_count++;
        }
    }

    lock->count++;

    return 1;
}


int lockfile_save(const LockFile *lock, const char *path)
{
    if (lock == NULL || path == NULL)
        return 0;

    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;

    fprintf(
        file,
        "# Generado por rivo install. No editar a mano.\n"
        "version = 1\n"
    );

    for (int i = 0; i < lock->count; i++) {

        const LockedPackage *package =
            &lock->packages[i];

        fprintf(
            file,
            "\n[[paquete]]\n"
            "nombre = \"%s\"\n"
            "version = \"%s\"\n"
            "origen = \"%s\"\n"
            "deps = [",
            package->name ? package->name : "",
            package->version ? package->version : "0.0.0",
            package->source ? package->source : ""
        );

        for (int j = 0; j < package->dep_count; j++) {

            fprintf(
                file,
                "%s\"%s\"",
                j > 0 ? ", " : "",
                package->deps[j]
            );
        }

        fprintf(file, "]\n");
    }

    int ok = !ferror(file);

    fclose(file);

    return ok;
}


/* ---------------- PARSER ---------------- */

static char *lock_take_quoted(char **cursor)
{
    char *text = *cursor;

    if (text == NULL || text[0] != '"')
        return NULL;

    char *closing = strchr(text + 1, '"');

    if (closing == NULL)
        return NULL;

    *closing = '\0';

    char *copy = lock_duplicate(text + 1);

    if (copy == NULL)
        return NULL;

    *cursor = closing + 1;

    return copy;
}


static int lock_parse_deps(
    char *value,
    char ***out_deps,
    int *out_count
)
{
    *out_deps = NULL;
    *out_count = 0;

    value = lock_trim(value);

    if (*value != '[')
        return 0;

    value++;

    char *close = strchr(value, ']');

    if (close == NULL)
        return 0;

    *close = '\0';

    char *cursor = value;

    while (cursor != NULL) {

        cursor = lock_trim(cursor);

        if (*cursor == '\0')
            break;

        if (*cursor != '"')
            return 0;

        char *text = lock_take_quoted(&cursor);

        if (text == NULL)
            return 0;

        char **grown =
            realloc(
                *out_deps,
                (size_t)(*out_count + 1) *
                    sizeof(char *)
            );

        if (grown == NULL) {
            free(text);
            return 0;
        }

        *out_deps = grown;
        (*out_deps)[(*out_count)++] = text;

        cursor = lock_trim(cursor);

        if (*cursor == ',') {
            cursor++;
            continue;
        }

        break;
    }

    return 1;
}


static int lock_parse_file(FILE *file, LockFile *lock)
{
    char line[4096];

    int header_seen = 0;
    int in_package = 0;
    int package_complete = 0;

    LockedPackage current;
    memset(&current, 0, sizeof(current));

    while (fgets(line, sizeof(line), file) != NULL) {

        char *text = lock_trim(line);

        if (*text == '\0' || *text == '#')
            continue;

        if (strcmp(text, "version = 1") == 0) {
            header_seen = 1;
            continue;
        }

        if (strcmp(text, "[[paquete]]") == 0) {

            if (in_package) {

                if (!package_complete) {
                    lockfile_free_package(&current);
                    return 0;
                }

                char **deps = current.deps;
                int dep_count = current.dep_count;

                current.deps = NULL;
                current.dep_count = 0;

                int ok = lockfile_add(
                    lock,
                    current.name,
                    current.version,
                    current.source,
                    (const char *const *) deps,
                    dep_count
                );

                lockfile_free_package(&current);
                memset(&current, 0, sizeof(current));

                if (!ok)
                    return 0;
            }

            memset(&current, 0, sizeof(current));
            in_package = 1;
            package_complete = 0;
            continue;
        }

        if (!in_package)
            return 0;

        char *equal = strchr(text, '=');

        if (equal == NULL)
            return 0;

        *equal = '\0';

        char *key = lock_trim(text);
        char *value = lock_trim(equal + 1);

        if (strcmp(key, "nombre") == 0 ||
            strcmp(key, "version") == 0 ||
            strcmp(key, "origen") == 0) {

            char *cursor = value;
            char *parsed = lock_take_quoted(&cursor);

            if (parsed == NULL)
                return 0;

            if (strcmp(key, "nombre") == 0)
                current.name = parsed;
            else if (strcmp(key, "version") == 0)
                current.version = parsed;
            else
                current.source = parsed;

        } else if (strcmp(key, "deps") == 0) {

            if (!lock_parse_deps(
                    value,
                    &current.deps,
                    &current.dep_count))
                return 0;

        } else {

            return 0;
        }

        package_complete =
            current.name != NULL &&
            current.version != NULL &&
            current.source != NULL;
    }

    if (in_package) {

        if (!package_complete) {
            lockfile_free_package(&current);
            return 0;
        }

        int ok = lockfile_add(
            lock,
            current.name,
            current.version,
            current.source,
            (const char *const *) current.deps,
            current.dep_count
        );

        lockfile_free_package(&current);

        if (!ok)
            return 0;
    }

    return header_seen;
}


LockFile *lockfile_load(const char *path)
{
    if (path == NULL)
        return NULL;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return NULL;

    LockFile *lock = lockfile_new();

    if (lock == NULL) {
        fclose(file);
        return NULL;
    }

    int ok = lock_parse_file(file, lock);

    fclose(file);

    if (!ok) {
        lockfile_destroy(lock);
        return NULL;
    }

    return lock;
}
