/* =========================================================
 * GESTOR DE PAQUETES LOCAL (v0.5)
 *
 * - Resuelve dependencias declaradas en rivo.toml.
 * - Las de tipo ruta se usan en su sitio.
 * - Las de tipo version se buscan en .rivo/deps/<nombre>
 *   o vendor/<nombre> (aun sin registro remoto).
 * - Genera rivo.lock con version y origen exactos.
 * ======================================================= */

#include "packages.h"
#include "manifest.h"
#include "lockfile.h"
#include "semver.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#include <sys/stat.h>
#define STAT _stat
#define IS_DIR(mode) (((mode) & _S_IFDIR) != 0)
#else
#include <dirent.h>
#include <sys/stat.h>
#define STAT stat
#define IS_DIR(mode) S_ISDIR(mode)
#endif

#ifndef PATH_MAX
#define PATH_MAX 32768
#endif

#define PACKAGES_MAX_DEPTH 32

typedef struct ResolveContext {

    const char *project_dir;
    LockFile *lock;

    const char *registry_url;
    int registry_refresh;

    char *errbuf;
    size_t errlen;

} ResolveContext;


typedef struct PackagesIndexEntry {

    char *name;
    char *version;
    char *git;
    char *tag;
    char *description;

} PackagesIndexEntry;


static int packages_valid_name(const char *name);


static char *packages_duplicate(const char *text)
{
    if (text == NULL)
        return NULL;

    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL)
        memcpy(copy, text, length);

    return copy;
}


static int packages_error(
    char *errbuf,
    size_t errlen,
    const char *format,
    ...
)
{
    if (errbuf != NULL && errlen > 0) {

        va_list arguments;
        va_start(arguments, format);
        vsnprintf(errbuf, errlen, format, arguments);
        va_end(arguments);
    }

    return 0;
}


static int packages_set_error(
    ResolveContext *context,
    const char *format,
    ...
)
{
    if (context != NULL &&
        context->errbuf != NULL &&
        context->errlen > 0) {

        va_list arguments;
        va_start(arguments, format);
        vsnprintf(
            context->errbuf,
            context->errlen,
            format,
            arguments
        );
        va_end(arguments);
    }

    return 0;
}


static int packages_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    if (path[0] == '/' || path[0] == '\\')
        return 1;

#if defined(_WIN32)
    if (path[1] == ':')
        return 1;
#endif

    return 0;
}


static char *packages_join(
    const char *directory,
    const char *name
)
{
    if (directory == NULL || name == NULL)
        return NULL;

    if (packages_path_is_absolute(name))
        return packages_duplicate(name);

    size_t length =
        strlen(directory) + strlen(name) + 2;

    char *path = malloc(length);

    if (path == NULL)
        return NULL;

    snprintf(
        path,
        length,
        "%s/%s",
        directory,
        name
    );

    return path;
}


static int packages_file_exists(const char *path)
{
    if (path == NULL)
        return 0;

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return 0;

    fclose(file);

    return 1;
}


static int packages_dir_exists(const char *path)
{
    if (path == NULL)
        return 0;

    struct STAT status;

    if (STAT(path, &status) != 0)
        return 0;

    return IS_DIR(status.st_mode);
}


static char *packages_read_file(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer = malloc((size_t) size + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t) size, file);

    fclose(file);

    buffer[read] = '\0';

    return buffer;
}


static int packages_write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;

    if (text != NULL)
        fputs(text, file);

    int ok = !ferror(file);

    fclose(file);

    return ok;
}


static int packages_make_dirs(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    char buffer[PATH_MAX];

    if (snprintf(
            buffer,
            sizeof(buffer),
            "%s",
            path) >= (int) sizeof(buffer))
        return 0;

    for (char *cursor = buffer + 1;
         *cursor != '\0';
         cursor++) {

        if (*cursor != '/' && *cursor != '\\')
            continue;

        char separator = *cursor;
        *cursor = '\0';

        if (buffer[0] != '\0') {
#if defined(_WIN32)
            _mkdir(buffer);
#else
            mkdir(buffer, 0755);
#endif
        }

        *cursor = separator;
    }

#if defined(_WIN32)
    if (_mkdir(buffer) == 0)
        return 1;
#else
    if (mkdir(buffer, 0755) == 0)
        return 1;
#endif

    return packages_dir_exists(buffer);
}


static char *packages_shell_quote(const char *text)
{
    if (text == NULL)
        return NULL;

#if defined(_WIN32)

    if (strchr(text, '"') != NULL)
        return NULL;

    size_t length = strlen(text) + 3;
    char *quoted = malloc(length);

    if (quoted == NULL)
        return NULL;

    snprintf(quoted, length, "\"%s\"", text);

    return quoted;

#else

    size_t extra = 3;

    for (const char *cursor = text;
         *cursor != '\0';
         cursor++) {

        if (*cursor == '\'')
            extra += 3;
    }

    char *quoted = malloc(strlen(text) + extra + 1);

    if (quoted == NULL)
        return NULL;

    char *out = quoted;
    *out++ = '\'';

    for (const char *cursor = text;
         *cursor != '\0';
         cursor++) {

        if (*cursor == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = *cursor;
        }
    }

    *out++ = '\'';
    *out = '\0';

    return quoted;

#endif
}


static void packages_remove_tree(const char *path)
{
    if (path == NULL || !packages_dir_exists(path))
        return;

    char command[PATH_MAX + 32];

#if defined(_WIN32)
    snprintf(
        command,
        sizeof(command),
        "rmdir /S /Q \"%s\"",
        path
    );
#else
    char *quoted = packages_shell_quote(path);

    if (quoted == NULL)
        return;

    snprintf(
        command,
        sizeof(command),
        "rm -rf %s",
        quoted
    );

    free(quoted);
#endif

    int ignored = system(command);
    (void) ignored;
}


static int packages_git_available(void)
{
#if defined(_WIN32)
    return system("git --version >NUL 2>&1") == 0;
#else
    return system("git --version >/dev/null 2>&1") == 0;
#endif
}


static int packages_git_clone(
    const char *url,
    const char *tag,
    const char *destination,
    char *errbuf,
    size_t errlen
)
{
    if (url == NULL || destination == NULL)
        return packages_error(
            errbuf,
            errlen,
            "datos de dependencia git incompletos"
        );

    if (strchr(url, '\n') != NULL ||
        (tag != NULL &&
         (strchr(tag, '\n') != NULL ||
          strchr(tag, ' ') != NULL ||
          strchr(tag, '\t') != NULL))) {

        return packages_error(
            errbuf,
            errlen,
            "URL o tag git no validos"
        );
    }

    if (!packages_git_available()) {

        return packages_error(
            errbuf,
            errlen,
            "se necesita git para instalar '%s'",
            url
        );
    }

    char *quoted_url = packages_shell_quote(url);
    char *quoted_tag =
        tag != NULL
            ? packages_shell_quote(tag)
            : packages_duplicate("");
    char *quoted_dest = packages_shell_quote(destination);

    if (quoted_url == NULL ||
        quoted_tag == NULL ||
        quoted_dest == NULL) {

        free(quoted_url);
        free(quoted_tag);
        free(quoted_dest);

        return packages_error(
            errbuf,
            errlen,
            "caracteres no admitidos en la dependencia git"
        );
    }

    packages_remove_tree(destination);

    char *parent = packages_duplicate(destination);

    if (parent != NULL) {

        char *slash = strrchr(parent, '/');

#if defined(_WIN32)
        char *backslash = strrchr(parent, '\\');

        if (backslash != NULL &&
            (slash == NULL || backslash > slash))
            slash = backslash;
#endif

        if (slash != NULL) {
            *slash = '\0';
            packages_make_dirs(parent);
        }

        free(parent);
    }

    size_t length =
        strlen(quoted_url) + strlen(quoted_tag) +
        strlen(quoted_dest) + 160;

    char *command = malloc(length);

    if (command == NULL) {

        free(quoted_url);
        free(quoted_tag);
        free(quoted_dest);

        return packages_error(
            errbuf,
            errlen,
            "memoria insuficiente"
        );
    }

    if (tag != NULL) {

        snprintf(
            command,
            length,
            "git -c advice.detachedHead=false clone "
            "--quiet --depth 1 --branch %s %s %s",
            quoted_tag,
            quoted_url,
            quoted_dest
        );

    } else {

        snprintf(
            command,
            length,
            "git clone --quiet --depth 1 %s %s",
            quoted_url,
            quoted_dest
        );
    }

    int result = system(command);

    free(command);
    free(quoted_url);
    free(quoted_tag);
    free(quoted_dest);

    if (result != 0) {

        packages_remove_tree(destination);

        if (tag != NULL)
            return packages_error(
                errbuf,
                errlen,
                "no se pudo clonar '%s' en el tag '%s'",
                url,
                tag
            );

        return packages_error(
            errbuf,
            errlen,
            "no se pudo clonar '%s'",
            url
        );
    }

    return 1;
}


static char *packages_trim_text(char *text)
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


static int packages_compare_strings(
    const void *left,
    const void *right
)
{
    const char *const *a = left;
    const char *const *b = right;

    return strcmp(*a, *b);
}


static int packages_read_dir(
    const char *path,
    char ***out,
    int *out_count
)
{
    *out = NULL;
    *out_count = 0;

#if defined(_WIN32)

    char pattern[PATH_MAX];

    if (snprintf(
            pattern,
            sizeof(pattern),
            "%s/*",
            path) >= (int) sizeof(pattern))
        return 0;

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE)
        return 1;

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0)
            continue;

        char **grown =
            realloc(
                *out,
                (size_t)(*out_count + 1) *
                    sizeof(char *)
            );

        if (grown == NULL)
            break;

        *out = grown;

        (*out)[*out_count] =
            packages_duplicate(data.cFileName);

        if ((*out)[*out_count] == NULL)
            break;

        (*out_count)++;

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

#else

    DIR *directory = opendir(path);

    if (directory == NULL)
        return 1;

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char **grown =
            realloc(
                *out,
                (size_t)(*out_count + 1) *
                    sizeof(char *)
            );

        if (grown == NULL)
            break;

        *out = grown;

        (*out)[*out_count] =
            packages_duplicate(entry->d_name);

        if ((*out)[*out_count] == NULL)
            break;

        (*out_count)++;
    }

    closedir(directory);

#endif

    if (*out_count > 1)
        qsort(
            *out,
            (size_t) *out_count,
            sizeof(char *),
            packages_compare_strings
        );

    return 1;
}


static void packages_free_dir_entries(
    char **entries,
    int count
)
{
    for (int i = 0; i < count; i++)
        free(entries[i]);

    free(entries);
}


static void packages_index_entry_clear(
    PackagesIndexEntry *entry
)
{
    if (entry == NULL)
        return;

    free(entry->name);
    free(entry->version);
    free(entry->git);
    free(entry->tag);
    free(entry->description);

    memset(entry, 0, sizeof(*entry));
}


static int packages_index_parse(
    const char *path,
    PackagesIndexEntry *out
)
{
    memset(out, 0, sizeof(*out));

    char *text = packages_read_file(path);

    if (text == NULL)
        return 0;

    int in_package = 0;

    for (char *cursor = text;
         cursor != NULL && *cursor != '\0'; ) {

        char *newline = strchr(cursor, '\n');

        if (newline != NULL) {
            *newline = '\0';
            newline++;
        }

        char *line = packages_trim_text(cursor);

        if (line[0] == '\0' || line[0] == '#') {
            cursor = newline;
            continue;
        }

        if (line[0] == '[') {

            in_package =
                strcmp(line, "[paquete]") == 0;

            cursor = newline;
            continue;
        }

        if (!in_package) {
            cursor = newline;
            continue;
        }

        char *equal = strchr(line, '=');

        if (equal == NULL) {
            cursor = newline;
            continue;
        }

        *equal = '\0';

        char *key = packages_trim_text(line);
        char *value = packages_trim_text(equal + 1);

        if (value[0] != '"') {
            cursor = newline;
            continue;
        }

        char *closing = strchr(value + 1, '"');

        if (closing == NULL) {
            cursor = newline;
            continue;
        }

        *closing = '\0';

        char *copy = packages_duplicate(value + 1);

        if (copy == NULL) {
            cursor = newline;
            continue;
        }

        char **slot = NULL;

        if (strcmp(key, "nombre") == 0)
            slot = &out->name;
        else if (strcmp(key, "version") == 0)
            slot = &out->version;
        else if (strcmp(key, "git") == 0)
            slot = &out->git;
        else if (strcmp(key, "tag") == 0)
            slot = &out->tag;
        else if (strcmp(key, "descripcion") == 0)
            slot = &out->description;

        if (slot != NULL) {
            free(*slot);
            *slot = copy;
        } else {
            free(copy);
        }

        cursor = newline;
    }

    free(text);

    if (out->name == NULL ||
        out->version == NULL ||
        out->git == NULL ||
        out->tag == NULL) {

        packages_index_entry_clear(out);
        return 0;
    }

    return 1;
}


static char *packages_registry_dir(
    ResolveContext *context
)
{
    if (context == NULL ||
        context->registry_url == NULL ||
        context->registry_url[0] == '\0') {

        packages_set_error(
            context,
            "no hay registro configurado; anade "
            "[registro] url = \"...\" a rivo.toml"
        );

        return NULL;
    }

    char *directory =
        packages_join(
            context->project_dir,
            ".rivo/registro"
        );

    if (directory == NULL) {
        packages_set_error(context, "memoria insuficiente");
        return NULL;
    }

    if (!packages_dir_exists(directory)) {

        char *parent =
            packages_join(context->project_dir, ".rivo");

        if (parent != NULL) {
            packages_make_dirs(parent);
            free(parent);
        }

        if (!packages_git_clone(
                context->registry_url,
                NULL,
                directory,
                context->errbuf,
                context->errlen)) {

            free(directory);
            return NULL;
        }

    } else if (context->registry_refresh) {

        char *quoted =
            packages_shell_quote(directory);

        if (quoted == NULL) {

            free(directory);

            packages_set_error(
                context,
                "memoria insuficiente"
            );

            return NULL;
        }

        size_t length = strlen(quoted) + 64;
        char *command = malloc(length);

        if (command == NULL) {

            free(quoted);
            free(directory);

            packages_set_error(
                context,
                "memoria insuficiente"
            );

            return NULL;
        }

        snprintf(
            command,
            length,
            "git -C %s pull --quiet --ff-only",
            quoted
        );

        int result = system(command);

        free(command);
        free(quoted);

        if (result != 0) {

            free(directory);

            packages_set_error(
                context,
                "no se pudo actualizar el registro"
            );

            return NULL;
        }
    }

    return directory;
}


static int packages_registry_find(
    ResolveContext *context,
    const char *name,
    const char *requirement,
    PackagesIndexEntry *out
)
{
    *out = (PackagesIndexEntry){0};

    char *registry = packages_registry_dir(context);

    if (registry == NULL)
        return 0;

    char *packages = packages_join(registry, "paquetes");

    char *package_dir =
        packages != NULL
            ? packages_join(packages, name)
            : NULL;

    free(packages);

    if (package_dir == NULL) {

        free(registry);

        packages_set_error(context, "memoria insuficiente");
        return 0;
    }

    char **entries = NULL;
    int entry_count = 0;

    int ok =
        packages_read_dir(
            package_dir,
            &entries,
            &entry_count
        );

    if (!ok) {

        free(package_dir);
        free(registry);

        packages_set_error(
            context,
            "no se pudo leer el indice del registro"
        );

        return 0;
    }

    SemVer best_version;
    int have_best = 0;

    for (int i = 0; i < entry_count; i++) {

        size_t length = strlen(entries[i]);

        if (length < 6 ||
            strcmp(entries[i] + length - 5, ".toml") != 0)
            continue;

        char *path = packages_join(package_dir, entries[i]);

        if (path == NULL)
            continue;

        PackagesIndexEntry candidate;

        if (!packages_index_parse(path, &candidate)) {
            free(path);
            continue;
        }

        free(path);

        SemVer version;

        if (!semver_parse(candidate.version, &version) ||
            semver_satisfies(
                requirement,
                &version) != 1) {

            packages_index_entry_clear(&candidate);
            continue;
        }

        if (!have_best ||
            semver_compare(&version, &best_version) > 0) {

            if (have_best)
                packages_index_entry_clear(out);

            *out = candidate;
            best_version = version;
            have_best = 1;

        } else {

            packages_index_entry_clear(&candidate);
        }
    }

    packages_free_dir_entries(entries, entry_count);

    free(package_dir);
    free(registry);

    return have_best;
}


static char *packages_manifest_path(const char *directory)
{
    return packages_join(directory, "rivo.toml");
}


static int packages_load_package(
    ResolveContext *context,
    const char *directory,
    ProjectManifest **out
)
{
    *out = NULL;

    char *path = packages_manifest_path(directory);

    if (path == NULL)
        return packages_set_error(
            context,
            "memoria insuficiente"
        );

    if (!packages_file_exists(path)) {
        free(path);
        return 1;
    }

    char errbuf[256];

    ManifestStatus status =
        manifest_load(
            directory,
            out,
            errbuf,
            sizeof(errbuf)
        );

    free(path);

    if (status == MANIFEST_INVALID) {

        return packages_set_error(
            context,
            "manifiesto invalido en '%s': %s",
            directory,
            errbuf
        );
    }

    if (status == MANIFEST_NOT_FOUND)
        *out = NULL;

    return 1;
}


static const char *packages_manifest_version(
    const ProjectManifest *manifest
)
{
    if (manifest != NULL &&
        manifest->version != NULL)
        return manifest->version;

    return "0.0.0";
}


static int packages_locate_version(
    ResolveContext *context,
    const char *name,
    const char *requirement,
    char *out_dir,
    size_t outlen
)
{
    char *roots[2];

    roots[0] = packages_join(
        context->project_dir,
        ".rivo/deps"
    );

    roots[1] = packages_join(
        context->project_dir,
        "vendor"
    );

    int found = 0;

    for (int i = 0; i < 2 && !found; i++) {

        if (roots[i] == NULL)
            continue;

        char *candidate = packages_join(roots[i], name);

        if (candidate == NULL)
            continue;

        if (packages_dir_exists(candidate)) {

            ProjectManifest *package = NULL;

            if (!packages_load_package(
                    context,
                    candidate,
                    &package)) {

                free(candidate);
                break;
            }

            if (package != NULL) {

                SemVer version;

                if (semver_parse(
                        packages_manifest_version(package),
                        &version)) {

                    int matches =
                        semver_satisfies(
                            requirement,
                            &version
                        );

                    if (matches == 1) {

                        snprintf(
                            out_dir,
                            outlen,
                            "%s",
                            candidate
                        );

                        found = 1;
                    }
                }

                manifest_destroy(package);
            }
        }

        free(candidate);
    }

    free(roots[0]);
    free(roots[1]);

    if (found || context->registry_url == NULL)
        return found;

    PackagesIndexEntry entry;

    if (!packages_registry_find(
            context,
            name,
            requirement,
            &entry)) {

        return 0;
    }

    char *deps_root =
        packages_join(context->project_dir, ".rivo/deps");

    char *destination =
        deps_root != NULL
            ? packages_join(deps_root, name)
            : NULL;

    if (deps_root == NULL || destination == NULL) {

        free(deps_root);
        free(destination);
        packages_index_entry_clear(&entry);

        packages_set_error(context, "memoria insuficiente");
        return 0;
    }

    int ok =
        packages_make_dirs(deps_root) &&
        packages_git_clone(
            entry.git,
            entry.tag,
            destination,
            context->errbuf,
            context->errlen);

    free(deps_root);

    if (!ok) {

        free(destination);
        packages_index_entry_clear(&entry);
        return 0;
    }

    ProjectManifest *package = NULL;

    if (!packages_load_package(
            context,
            destination,
            &package)) {

        packages_index_entry_clear(&entry);
        free(destination);
        return 0;
    }

    if (package != NULL &&
        strcmp(
            packages_manifest_version(package),
            entry.version) != 0) {

        packages_set_error(
            context,
            "el registro anuncia '%s' %s pero el "
            "repositorio contiene %s",
            name,
            entry.version,
            packages_manifest_version(package)
        );

        manifest_destroy(package);
        packages_index_entry_clear(&entry);
        free(destination);
        return 0;
    }

    manifest_destroy(package);

    snprintf(
        out_dir,
        outlen,
        "%s",
        destination
    );

    free(destination);
    packages_index_entry_clear(&entry);

    return 1;
}


static int packages_resolve_manifest(
    ResolveContext *context,
    ProjectManifest *manifest,
    int depth
)
{
    if (manifest == NULL)
        return 1;

    if (depth > PACKAGES_MAX_DEPTH) {

        return packages_set_error(
            context,
            "dependencias demasiado profundas "
            "(posible ciclo) en '%s'",
            manifest->dir ? manifest->dir : "?"
        );
    }

    for (int i = 0;
         i < manifest->dependency_count;
         i++) {

        ManifestDependency *dependency =
            &manifest->dependencies[i];

        LockedPackage *existing =
            lockfile_find(
                context->lock,
                dependency->name
            );

        if (existing != NULL) {

            if (dependency->kind ==
                    MANIFEST_DEP_VERSION) {

                SemVer locked;

                if (semver_parse(
                        existing->version,
                        &locked)) {

                    int matches =
                        semver_satisfies(
                            dependency->requirement,
                            &locked
                        );

                    if (matches == 0) {

                        return packages_set_error(
                            context,
                            "conflicto de versiones: "
                            "'%s' requiere '%s' pero "
                            "rivo.lock fija %s",
                            dependency->name,
                            dependency->requirement,
                            existing->version
                        );
                    }
                }
            }

            continue;
        }

        char package_dir[PATH_MAX];
        package_dir[0] = '\0';

        if (dependency->kind == MANIFEST_DEP_PATH) {

            char *resolved =
                packages_join(
                    manifest->dir,
                    dependency->path
                );

            if (resolved == NULL) {

                return packages_set_error(
                    context,
                    "memoria insuficiente"
                );
            }

            if (!packages_dir_exists(resolved)) {

                packages_set_error(
                    context,
                    "la dependencia '%s' apunta a '%s', "
                    "que no existe",
                    dependency->name,
                    resolved
                );

                free(resolved);
                return 0;
            }

            snprintf(
                package_dir,
                sizeof(package_dir),
                "%s",
                resolved
            );

            free(resolved);

        } else if (dependency->kind ==
                       MANIFEST_DEP_VERSION) {

            if (!packages_locate_version(
                    context,
                    dependency->name,
                    dependency->requirement,
                    package_dir,
                    sizeof(package_dir))) {

                return packages_set_error(
                    context,
                    "no se encontro '%s' que cumpla '%s' "
                    "en .rivo/deps, vendor ni en el registro",
                    dependency->name,
                    dependency->requirement
                );
            }

        } else {

            char *deps_root =
                packages_join(
                    context->project_dir,
                    ".rivo/deps"
                );

            char *destination =
                deps_root != NULL
                    ? packages_join(
                          deps_root,
                          dependency->name)
                    : NULL;

            if (deps_root == NULL ||
                destination == NULL) {

                free(deps_root);
                free(destination);

                return packages_set_error(
                    context,
                    "memoria insuficiente"
                );
            }

            int ok =
                packages_make_dirs(deps_root) &&
                packages_git_clone(
                    dependency->git,
                    dependency->git_ref,
                    destination,
                    context->errbuf,
                    context->errlen);

            if (!ok) {

                free(deps_root);
                free(destination);

                return 0;
            }

            snprintf(
                package_dir,
                sizeof(package_dir),
                "%s",
                destination
            );

            free(deps_root);
            free(destination);
        }

        ProjectManifest *package = NULL;

        if (!packages_load_package(
                context,
                package_dir,
                &package)) {

            return 0;
        }

        if (dependency->kind == MANIFEST_DEP_GIT &&
            package != NULL &&
            package->nombre != NULL &&
            strcmp(
                package->nombre,
                dependency->name) != 0) {

            packages_set_error(
                context,
                "el repositorio git de '%s' contiene el "
                "paquete '%s'",
                dependency->name,
                package->nombre
            );

            manifest_destroy(package);
            return 0;
        }

        const char *version =
            packages_manifest_version(package);

        SemVer parsed;

        if (!semver_parse(version, &parsed)) {

            packages_set_error(
                context,
                "el paquete '%s' tiene una version no "
                "valida: '%s'",
                dependency->name,
                version
            );

            manifest_destroy(package);
            return 0;
        }

        if (dependency->kind == MANIFEST_DEP_VERSION) {

            int matches =
                semver_satisfies(
                    dependency->requirement,
                    &parsed
                );

            if (matches == 0) {

                packages_set_error(
                    context,
                    "'%s' version %s no cumple '%s'",
                    dependency->name,
                    version,
                    dependency->requirement
                );

                manifest_destroy(package);
                return 0;
            }
        }

        char source[PATH_MAX];

        snprintf(
            source,
            sizeof(source),
            "ruta:%s",
            package_dir
        );

        int child_count =
            package != NULL
                ? package->dependency_count
                : 0;

        const char **child_names = NULL;

        if (child_count > 0) {

            child_names =
                calloc(
                    (size_t) child_count,
                    sizeof(char *)
                );

            if (child_names == NULL) {

                manifest_destroy(package);

                return packages_set_error(
                    context,
                    "memoria insuficiente"
                );
            }

            for (int j = 0; j < child_count; j++) {

                child_names[j] =
                    package->dependencies[j].name;
            }
        }

        int added =
            lockfile_add(
                context->lock,
                dependency->name,
                version,
                source,
                (const char *const *) child_names,
                child_count
            );

        free(child_names);

        if (!added) {

            manifest_destroy(package);

            return packages_set_error(
                context,
                "no se pudo anadir '%s' (duplicado o "
                "sin memoria)",
                dependency->name
            );
        }

        if (package != NULL) {

            int ok =
                packages_resolve_manifest(
                    context,
                    package,
                    depth + 1
                );

            manifest_destroy(package);

            if (!ok)
                return 0;
        }
    }

    return 1;
}


static void packages_sort_lock(LockFile *lock)
{
    for (int i = 1; i < lock->count; i++) {

        LockedPackage current = lock->packages[i];

        int j = i - 1;

        while (j >= 0 &&
               strcmp(
                   lock->packages[j].name,
                   current.name) > 0) {

            lock->packages[j + 1] =
                lock->packages[j];

            j--;
        }

        lock->packages[j + 1] = current;
    }
}


static int packages_load_project(
    ProjectManifest **out,
    char *errbuf,
    size_t errlen
)
{
    ManifestStatus status =
        manifest_load(
            ".",
            out,
            errbuf,
            errlen
        );

    if (status == MANIFEST_NOT_FOUND) {

        packages_error(
            errbuf,
            errlen,
            "no se encontro rivo.toml en este "
            "directorio ni en sus padres"
        );

        return 0;
    }

    if (status == MANIFEST_INVALID)
        return 0;

    return 1;
}


static int packages_regenerate_lock(
    ProjectManifest *manifest,
    int refresh_registry,
    char *errbuf,
    size_t errlen
)
{
    LockFile *lock = lockfile_new();

    if (lock == NULL) {

        packages_error(
            errbuf,
            errlen,
            "memoria insuficiente"
        );

        return 0;
    }

    ResolveContext context;

    context.project_dir = manifest->dir;
    context.lock = lock;
    context.registry_url = manifest->registro_url;
    context.registry_refresh = refresh_registry;
    context.errbuf = errbuf;
    context.errlen = errlen;

    int ok =
        packages_resolve_manifest(
            &context,
            manifest,
            0
        );

    if (ok) {

        packages_sort_lock(lock);

        char *path =
            packages_join(
                manifest->dir,
                RIVO_LOCK_FILENAME
            );

        if (path == NULL) {

            packages_error(
                errbuf,
                errlen,
                "memoria insuficiente"
            );

            ok = 0;

        } else {

            if (!lockfile_save(lock, path)) {

                packages_error(
                    errbuf,
                    errlen,
                    "no se pudo escribir '%s'",
                    path
                );

                ok = 0;
            }

            free(path);
        }
    }

    int count = lock->count;

    lockfile_destroy(lock);

    if (ok) {

        printf(
            "Dependencias resueltas: %d\n"
            "rivo.lock actualizado\n",
            count
        );
    }

    return ok;
}


/* ---------------- EDICION DE rivo.toml ---------------- */

static int packages_line_is_section(
    const char *line,
    const char *section
)
{
    while (*line == ' ' || *line == '\t')
        line++;

    if (line[0] != '[')
        return 0;

    const char *close = strchr(line, ']');

    if (close == NULL)
        return 0;

    size_t length = (size_t)(close - line - 1);

    return strlen(section) == length &&
        strncmp(line + 1, section, length) == 0;
}


static int packages_line_dependency_name(
    const char *line,
    char *out,
    size_t outlen
)
{
    while (*line == ' ' || *line == '\t')
        line++;

    if (line[0] == '#' || line[0] == '\0' ||
        line[0] == '[')
        return 0;

    const char *equal = strchr(line, '=');

    if (equal == NULL)
        return 0;

    const char *end = equal;

    while (end > line &&
           (end[-1] == ' ' || end[-1] == '\t'))
        end--;

    size_t length = (size_t)(end - line);

    if (length == 0 || length >= outlen)
        return 0;

    memcpy(out, line, length);
    out[length] = '\0';

    return 1;
}


static char *packages_toml_add(
    const char *text,
    const char *name,
    const char *value,
    int raw_value,
    char *errbuf,
    size_t errlen
)
{
    size_t capacity = strlen(text) + strlen(name) +
        strlen(value) + 128;

    char *output = malloc(capacity);

    if (output == NULL) {
        packages_error(errbuf, errlen, "memoria insuficiente");
        return NULL;
    }

    output[0] = '\0';

    char *copy = packages_duplicate(text);

    if (copy == NULL) {
        free(output);
        packages_error(errbuf, errlen, "memoria insuficiente");
        return NULL;
    }

    char *scan = packages_duplicate(copy);

    if (scan == NULL) {
        free(copy);
        free(output);
        packages_error(errbuf, errlen, "memoria insuficiente");
        return NULL;
    }

    int in_dependencies = 0;
    int section_found = 0;
    int already = 0;

    for (char *cursor = scan;
         cursor != NULL && *cursor != '\0'; ) {

        char *newline = strchr(cursor, '\n');

        if (newline != NULL) {
            *newline = '\0';
            newline++;
        }

        if (packages_line_is_section(
                cursor,
                "dependencias")) {

            in_dependencies = 1;
            section_found = 1;

        } else if (cursor[0] == '[') {

            in_dependencies = 0;
        }

        char key[256];

        if (in_dependencies &&
            packages_line_dependency_name(
                cursor,
                key,
                sizeof(key)) &&
            strcmp(key, name) == 0) {

            already = 1;
        }

        cursor = newline;
    }

    free(scan);

    if (already) {

        free(copy);
        free(output);

        packages_error(
            errbuf,
            errlen,
            "la dependencia '%s' ya existe en rivo.toml",
            name
        );

        return NULL;
    }

    in_dependencies = 0;
    int inserted = 0;

    for (char *cursor = copy;
         cursor != NULL && *cursor != '\0'; ) {

        char *newline = strchr(cursor, '\n');

        if (newline != NULL) {
            *newline = '\0';
            newline++;
        }

        size_t needed =
            strlen(output) + strlen(cursor) +
            strlen(name) + strlen(value) + 32;

        if (needed > capacity) {

            capacity = needed * 2;

            char *grown = realloc(output, capacity);

            if (grown == NULL) {
                free(copy);
                free(output);
                packages_error(
                    errbuf,
                    errlen,
                    "memoria insuficiente"
                );
                return NULL;
            }

            output = grown;
        }

        strcat(output, cursor);

        if (!inserted &&
            packages_line_is_section(
                cursor,
                "dependencias")) {

            strcat(output, "\n");
            strcat(output, name);
            strcat(output, " = ");

            if (!raw_value)
                strcat(output, "\"");

            strcat(output, value);

            if (!raw_value)
                strcat(output, "\"");

            inserted = 1;
            in_dependencies = 1;

        } else if (cursor[0] == '[') {

            in_dependencies = 0;
        }

        (void) in_dependencies;

        strcat(output, "\n");

        cursor = newline;
    }

    if (!inserted) {

        size_t needed =
            strlen(output) + strlen(name) +
            strlen(value) + 64;

        if (needed > capacity) {

            char *grown = realloc(output, needed + 1);

            if (grown == NULL) {
                free(copy);
                free(output);
                packages_error(
                    errbuf,
                    errlen,
                    "memoria insuficiente"
                );
                return NULL;
            }

            output = grown;
        }

        if (output[0] != '\0' && !section_found)
            strcat(output, "\n");

        if (!section_found)
            strcat(output, "[dependencias]\n");

        strcat(output, name);
        strcat(output, " = ");

        if (!raw_value)
            strcat(output, "\"");

        strcat(output, value);

        if (!raw_value)
            strcat(output, "\"");

        strcat(output, "\n");
    }

    free(copy);

    return output;
}


static char *packages_toml_remove(
    const char *text,
    const char *name,
    char *errbuf,
    size_t errlen
)
{
    char *copy = packages_duplicate(text);

    if (copy == NULL) {
        packages_error(errbuf, errlen, "memoria insuficiente");
        return NULL;
    }

    char *output = malloc(strlen(text) + 1);

    if (output == NULL) {
        free(copy);
        packages_error(errbuf, errlen, "memoria insuficiente");
        return NULL;
    }

    output[0] = '\0';

    int in_dependencies = 0;
    int removed = 0;

    for (char *cursor = copy;
         cursor != NULL && *cursor != '\0'; ) {

        char *newline = strchr(cursor, '\n');

        if (newline != NULL) {
            *newline = '\0';
            newline++;
        }

        if (packages_line_is_section(
                cursor,
                "dependencias")) {

            in_dependencies = 1;

        } else if (cursor[0] == '[') {

            in_dependencies = 0;
        }

        char key[256];
        int matches = 0;

        if (in_dependencies &&
            packages_line_dependency_name(
                cursor,
                key,
                sizeof(key)) &&
            strcmp(key, name) == 0) {

            matches = 1;
            removed = 1;
        }

        if (!matches) {

            strcat(output, cursor);
            strcat(output, "\n");
        }

        cursor = newline;
    }

    free(copy);

    if (!removed) {

        free(output);

        packages_error(
            errbuf,
            errlen,
            "la dependencia '%s' no esta en rivo.toml",
            name
        );

        return NULL;
    }

    return output;
}


/* ---------------- SUBCOMANDOS ---------------- */

static int packages_command_install_impl(int refresh)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    int ok =
        packages_regenerate_lock(
            manifest,
            refresh,
            errbuf,
            sizeof(errbuf)
        );

    if (!ok)
        fprintf(stderr, "rivo: %s\n", errbuf);

    manifest_destroy(manifest);

    return ok ? 0 : 1;
}


static int packages_command_install(void)
{
    return packages_command_install_impl(0);
}


#define PACKAGES_TREE_MAX_DEPTH 32

static void packages_tree_print_children(
    LockFile *lock,
    const LockedPackage *parent,
    int depth,
    const char **trail,
    int trail_count
)
{
    if (depth >= PACKAGES_TREE_MAX_DEPTH)
        return;

    for (int i = 0; i < parent->dep_count; i++) {

        const char *name = parent->deps[i];
        LockedPackage *child = lockfile_find(lock, name);

        for (int level = 0; level < depth; level++)
            printf("|   ");

        printf(
            "`-- %s %s\n",
            name,
            child ? child->version : "?"
        );

        int cycle = 0;

        for (int j = 0; j < trail_count; j++) {

            if (trail[j] != NULL &&
                strcmp(trail[j], name) == 0) {

                cycle = 1;
                break;
            }
        }

        if (child == NULL || cycle)
            continue;

        trail[trail_count] = name;

        packages_tree_print_children(
            lock,
            child,
            depth + 1,
            trail,
            trail_count + 1
        );
    }
}


static int packages_command_tree(void)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    char *lock_path =
        packages_join(
            manifest->dir,
            RIVO_LOCK_FILENAME
        );

    LockFile *lock =
        lock_path ? lockfile_load(lock_path) : NULL;

    if (lock == NULL) {

        fprintf(
            stderr,
            "rivo: no hay rivo.lock valido; "
            "ejecuta `rivo install`\n"
        );

        free(lock_path);
        manifest_destroy(manifest);

        return 1;
    }

    printf(
        "%s %s\n",
        manifest->nombre ? manifest->nombre : "proyecto",
        packages_manifest_version(manifest)
    );

    for (int i = 0;
         i < manifest->dependency_count;
         i++) {

        const ManifestDependency *dependency =
            &manifest->dependencies[i];

        LockedPackage *package =
            lockfile_find(lock, dependency->name);

        printf(
            "|-- %s %s (%s)\n",
            dependency->name,
            package ? package->version : "?",
            package ? package->source : "?"
        );

        if (package == NULL)
            continue;

        const char *trail[PACKAGES_TREE_MAX_DEPTH];
        trail[0] = dependency->name;

        packages_tree_print_children(
            lock,
            package,
            1,
            trail,
            1
        );
    }

    fflush(stdout);

    lockfile_destroy(lock);
    free(lock_path);
    manifest_destroy(manifest);

    return 0;
}


static int packages_command_list(void)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    char *lock_path =
        packages_join(
            manifest->dir,
            RIVO_LOCK_FILENAME
        );

    LockFile *lock =
        lock_path ? lockfile_load(lock_path) : NULL;

    if (lock == NULL) {

        fprintf(
            stderr,
            "rivo: no hay rivo.lock valido; "
            "ejecuta `rivo install`\n"
        );

        free(lock_path);
        manifest_destroy(manifest);

        return 1;
    }

    for (int i = 0; i < lock->count; i++) {

        printf(
            "%s %s %s\n",
            lock->packages[i].name,
            lock->packages[i].version,
            lock->packages[i].source
        );
    }

    fflush(stdout);

    lockfile_destroy(lock);
    free(lock_path);
    manifest_destroy(manifest);

    return 0;
}


static int packages_contains_ci(
    const char *text,
    const char *needle
)
{
    if (text == NULL || needle == NULL)
        return 0;

    size_t needle_length = strlen(needle);

    if (needle_length == 0)
        return 1;

    for (const char *start = text; *start; start++) {

        size_t i = 0;

        while (i < needle_length &&
               start[i] != '\0' &&
               tolower((unsigned char) start[i]) ==
                   tolower((unsigned char) needle[i]))
            i++;

        if (i == needle_length)
            return 1;
    }

    return 0;
}


static int packages_command_search(const char *text)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    ResolveContext context = {
        .project_dir = manifest->dir,
        .lock = NULL,
        .registry_url = manifest->registro_url,
        .registry_refresh = 0,
        .errbuf = errbuf,
        .errlen = sizeof(errbuf)
    };

    char *registry = packages_registry_dir(&context);

    if (registry == NULL) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        manifest_destroy(manifest);
        return 1;
    }

    char *packages = packages_join(registry, "paquetes");
    char **entries = NULL;
    int entry_count = 0;

    if (packages == NULL ||
        !packages_read_dir(packages, &entries, &entry_count)) {

        fprintf(
            stderr,
            "rivo: no se pudo leer el indice del registro\n"
        );

        free(packages);
        free(registry);
        manifest_destroy(manifest);

        return 1;
    }

    int found = 0;

    for (int i = 0; i < entry_count; i++) {

        PackagesIndexEntry entry;

        if (!packages_registry_find(
                &context,
                entries[i],
                "*",
                &entry))
            continue;

        if (packages_contains_ci(entry.name, text) ||
            packages_contains_ci(entry.description, text)) {

            printf(
                "%s %s",
                entry.name,
                entry.version
            );

            if (entry.description != NULL)
                printf(" - %s", entry.description);

            printf("\n");
            found++;
        }

        packages_index_entry_clear(&entry);
    }

    if (!found)
        printf("Sin resultados para '%s'\n", text);

    fflush(stdout);

    packages_free_dir_entries(entries, entry_count);
    free(packages);
    free(registry);
    manifest_destroy(manifest);

    return 0;
}


static int packages_command_info(const char *name)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    ResolveContext context = {
        .project_dir = manifest->dir,
        .lock = NULL,
        .registry_url = manifest->registro_url,
        .registry_refresh = 0,
        .errbuf = errbuf,
        .errlen = sizeof(errbuf)
    };

    char *registry = packages_registry_dir(&context);

    if (registry == NULL) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        manifest_destroy(manifest);
        return 1;
    }

    char *packages = packages_join(registry, "paquetes");
    char *package_dir =
        packages != NULL
            ? packages_join(packages, name)
            : NULL;

    free(packages);

    char **entries = NULL;
    int entry_count = 0;

    if (package_dir == NULL ||
        !packages_read_dir(
            package_dir,
            &entries,
            &entry_count) ||
        entry_count == 0) {

        fprintf(
            stderr,
            "rivo: no hay versiones de '%s' en el registro\n",
            name
        );

        free(package_dir);
        free(registry);
        manifest_destroy(manifest);

        return 1;
    }

    printf("%s\n", name);

    for (int i = 0; i < entry_count; i++) {

        size_t length = strlen(entries[i]);

        if (length < 6 ||
            strcmp(entries[i] + length - 5, ".toml") != 0)
            continue;

        char *path = packages_join(package_dir, entries[i]);

        if (path == NULL)
            continue;

        PackagesIndexEntry entry;

        if (packages_index_parse(path, &entry)) {

            printf(
                "  %s  %s#%s\n",
                entry.version,
                entry.git,
                entry.tag
            );

            if (entry.description != NULL)
                printf("      %s\n", entry.description);

            packages_index_entry_clear(&entry);
        }

        free(path);
    }

    fflush(stdout);

    packages_free_dir_entries(entries, entry_count);
    free(package_dir);
    free(registry);
    manifest_destroy(manifest);

    return 0;
}


static char *packages_capture_command(const char *command)
{
#if defined(_WIN32)
    FILE *pipe = _popen(command, "r");
#else
    FILE *pipe = popen(command, "r");
#endif

    if (pipe == NULL)
        return NULL;

    char buffer[4096];
    size_t used = 0;

    size_t read =
        fread(
            buffer,
            1,
            sizeof(buffer) - 1,
            pipe
        );

    used = read;
    buffer[used] = '\0';

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    char *text = packages_trim_text(buffer);

    return packages_duplicate(text);
}


static int packages_command_publish(const char *tag)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    if (manifest->registro_url == NULL) {

        fprintf(
            stderr,
            "rivo: falta [registro] url en rivo.toml\n"
        );

        manifest_destroy(manifest);
        return 1;
    }

    if (manifest->nombre == NULL ||
        manifest->version == NULL) {

        fprintf(
            stderr,
            "rivo: el proyecto necesita nombre y version "
            "en [proyecto]\n"
        );

        manifest_destroy(manifest);
        return 1;
    }

    if (!packages_valid_name(manifest->nombre)) {

        fprintf(
            stderr,
            "rivo: nombre de paquete no valido: '%s'\n",
            manifest->nombre
        );

        manifest_destroy(manifest);
        return 1;
    }

    if (strchr(tag, ' ') != NULL ||
        strchr(tag, '\t') != NULL ||
        strchr(tag, '"') != NULL ||
        strchr(tag, '\'') != NULL) {

        fprintf(stderr, "rivo: tag no valido\n");
        manifest_destroy(manifest);
        return 1;
    }

    char *quoted_dir =
        packages_shell_quote(manifest->dir);

    char *quoted_tag = packages_shell_quote(tag);

    if (quoted_dir == NULL || quoted_tag == NULL) {

        free(quoted_dir);
        free(quoted_tag);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        manifest_destroy(manifest);
        return 1;
    }

    size_t check_length =
        strlen(quoted_dir) + strlen(quoted_tag) + 96;

    char *check = malloc(check_length);

    if (check == NULL) {

        free(quoted_dir);
        free(quoted_tag);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        manifest_destroy(manifest);
        return 1;
    }

#if defined(_WIN32)
    const char *redirect = " >NUL 2>&1";
#else
    const char *redirect = " >/dev/null 2>&1";
#endif

    snprintf(
        check,
        check_length,
        "git -C %s rev-parse --verify --quiet "
        "refs/tags/%s%s",
        quoted_dir,
        quoted_tag,
        redirect
    );

    int tag_exists = system(check) == 0;

    free(check);

    if (!tag_exists) {

        fprintf(
            stderr,
            "rivo: el tag '%s' no existe en este repositorio\n",
            tag
        );

        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        return 1;
    }

    size_t remote_length =
        strlen(quoted_dir) + 96;

    char *remote_command = malloc(remote_length);

    if (remote_command == NULL) {

        free(quoted_dir);
        free(quoted_tag);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        manifest_destroy(manifest);
        return 1;
    }

    snprintf(
        remote_command,
        remote_length,
        "git -C %s config --get remote.origin.url",
        quoted_dir
    );

    char *remote = packages_capture_command(remote_command);

    free(remote_command);

    if (remote == NULL || remote[0] == '\0') {

        fprintf(
            stderr,
            "rivo: el repositorio necesita un remoto "
            "'origin'\n"
        );

        free(remote);
        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        return 1;
    }

    char *publish_dir =
        packages_join(manifest->dir, ".rivo/publicacion");

    char *publish_parent =
        packages_join(manifest->dir, ".rivo");

    if (publish_dir == NULL || publish_parent == NULL) {

        free(publish_dir);
        free(publish_parent);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        manifest_destroy(manifest);
        return 1;
    }

    packages_make_dirs(publish_parent);
    free(publish_parent);

    int ok =
        packages_git_clone(
            manifest->registro_url,
            NULL,
            publish_dir,
            errbuf,
            sizeof(errbuf));

    if (!ok) {

        fprintf(stderr, "rivo: %s\n", errbuf);

        free(publish_dir);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        return 1;
    }

    char *package_index =
        packages_join(publish_dir, "paquetes");

    char *package_dir =
        package_index != NULL
            ? packages_join(package_index, manifest->nombre)
            : NULL;

    free(package_index);

    if (package_dir == NULL) {

        free(package_dir);
        free(publish_dir);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        manifest_destroy(manifest);
        return 1;
    }

    packages_make_dirs(package_dir);

    size_t entry_path_length =
        strlen(package_dir) + strlen(manifest->version) + 8;

    char *entry =
        malloc(entry_path_length);

    char *content = NULL;

    if (entry != NULL) {

        snprintf(
            entry,
            entry_path_length,
            "%s/%s.toml",
            package_dir,
            manifest->version
        );

        size_t content_length =
            strlen(manifest->nombre) +
            strlen(manifest->version) +
            strlen(remote) + strlen(tag) + 128;

        content = malloc(content_length);

        if (content != NULL) {

            snprintf(
                content,
                content_length,
                "[paquete]\n"
                "nombre = \"%s\"\n"
                "version = \"%s\"\n"
                "git = \"%s\"\n"
                "tag = \"%s\"\n",
                manifest->nombre,
                manifest->version,
                remote,
                tag
            );
        }
    }

    if (entry != NULL && packages_file_exists(entry)) {

        fprintf(
            stderr,
            "rivo: %s %s ya esta publicado en el registro\n",
            manifest->nombre,
            manifest->version
        );

        free(entry);
        free(content);
        free(package_dir);
        free(publish_dir);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        return 1;
    }

    if (entry == NULL || content == NULL ||
        !packages_write_file(entry, content)) {

        fprintf(
            stderr,
            "rivo: no se pudo escribir la entrada del "
            "registro\n"
        );

        free(entry);
        free(content);
        free(package_dir);
        free(publish_dir);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        return 1;
    }

    free(entry);
    free(content);

    char *quoted_publish =
        packages_shell_quote(publish_dir);

    if (quoted_publish == NULL) {

        free(package_dir);
        free(publish_dir);
        free(remote);
        free(quoted_dir);
        free(quoted_tag);
        manifest_destroy(manifest);

        fprintf(stderr, "rivo: memoria insuficiente\n");
        return 1;
    }

    size_t command_length =
        strlen(quoted_publish) + 256;

    char *command = malloc(command_length);

    int pushed = 0;

    if (command != NULL) {

        snprintf(
            command,
            command_length,
            "git -C %s add . && "
            "git -C %s -c user.name=Rivo "
            "-c user.email=rivo@localhost "
            "commit -qm 'Publicar %s %s' && "
            "git -C %s push --quiet origin HEAD",
            quoted_publish,
            quoted_publish,
            manifest->nombre,
            manifest->version,
            quoted_publish
        );

        pushed = system(command) == 0;

        free(command);
    }

    free(quoted_publish);

    packages_remove_tree(publish_dir);

    char *cache =
        packages_join(manifest->dir, ".rivo/registro");

    packages_remove_tree(cache);
    free(cache);

    if (!pushed) {

        fprintf(
            stderr,
            "rivo: no se pudo publicar en '%s' "
            "(revisa credenciales y el remoto)\n",
            manifest->registro_url
        );
    } else {

        printf(
            "Publicado %s %s (tag %s)\n",
            manifest->nombre,
            manifest->version,
            tag
        );
    }

    free(package_dir);
    free(publish_dir);
    free(remote);
    free(quoted_dir);
    free(quoted_tag);
    manifest_destroy(manifest);

    return pushed ? 0 : 1;
}


static int packages_valid_name(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return 0;

    for (const char *cursor = name; *cursor; cursor++) {

        int ok =
            (*cursor >= 'a' && *cursor <= 'z') ||
            (*cursor >= 'A' && *cursor <= 'Z') ||
            (*cursor >= '0' && *cursor <= '9') ||
            *cursor == '_';

        if (!ok)
            return 0;
    }

    return 1;
}


static const char *packages_basename(const char *path)
{
    const char *base = path;

    for (const char *cursor = path; *cursor; cursor++) {

        if (*cursor == '/' || *cursor == '\\')
            base = cursor + 1;
    }

    return base;
}


static int packages_command_add(const char *argument)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    char name[256];
    name[0] = '\0';

    const char *value = argument;

    int is_git =
        strstr(argument, "://") != NULL ||
        strncmp(argument, "git@", 4) == 0;

    int raw_value = 0;
    char git_value[PATH_MAX * 2];

    int looks_like_path =
        !is_git &&
        (argument[0] == '.' ||
         argument[0] == '/' ||
         argument[0] == '\\' ||
         strchr(argument, '/') != NULL ||
         strchr(argument, '\\') != NULL);

    if (is_git) {

        const char *hash = strrchr(argument, '#');

        if (hash == NULL || hash == argument ||
            hash[1] == '\0') {

            fprintf(
                stderr,
                "rivo: usa `rivo add <url-git>#<tag>` "
                "para fijar una version reproducible\n"
            );

            manifest_destroy(manifest);
            return 1;
        }

        size_t url_length = (size_t)(hash - argument);
        const char *tag = hash + 1;

        if (strchr(tag, ' ') != NULL ||
            strchr(tag, '\t') != NULL ||
            strchr(tag, '"') != NULL ||
            strlen(tag) >= sizeof(git_value) / 2) {

            fprintf(
                stderr,
                "rivo: tag git no valido: '%s'\n",
                tag
            );

            manifest_destroy(manifest);
            return 1;
        }

        char *url = malloc(url_length + 1);

        if (url == NULL) {

            fprintf(stderr, "rivo: memoria insuficiente\n");
            manifest_destroy(manifest);
            return 1;
        }

        memcpy(url, argument, url_length);
        url[url_length] = '\0';

        char *probe =
            packages_join(
                manifest->dir,
                ".rivo/deps/.rivo-add-probe"
            );

        char *probe_parent =
            packages_join(manifest->dir, ".rivo/deps");

        if (probe == NULL || probe_parent == NULL) {

            free(probe);
            free(probe_parent);
            free(url);

            fprintf(stderr, "rivo: memoria insuficiente\n");
            manifest_destroy(manifest);
            return 1;
        }

        packages_make_dirs(probe_parent);
        free(probe_parent);

        ProjectManifest *probed = NULL;

        int cloned =
            packages_git_clone(
                url,
                tag,
                probe,
                errbuf,
                sizeof(errbuf));

        ManifestStatus probed_status =
            cloned
                ? manifest_load(
                      probe,
                      &probed,
                      errbuf,
                      sizeof(errbuf))
                : MANIFEST_NOT_FOUND;

        packages_remove_tree(probe);
        free(probe);

        if (!cloned ||
            probed_status != MANIFEST_OK ||
            probed == NULL ||
            probed->nombre == NULL) {

            fprintf(
                stderr,
                "rivo: no se pudo leer el paquete git: %s\n",
                errbuf
            );

            manifest_destroy(probed);
            free(url);
            manifest_destroy(manifest);
            return 1;
        }

        snprintf(
            name,
            sizeof(name),
            "%s",
            probed->nombre
        );

        manifest_destroy(probed);

        snprintf(
            git_value,
            sizeof(git_value),
            "{ git = \"%s\", tag = \"%s\" }",
            url,
            tag
        );

        free(url);

        value = git_value;
        raw_value = 1;

    } else if (looks_like_path) {

        snprintf(
            name,
            sizeof(name),
            "%s",
            packages_basename(argument)
        );

        char *resolved =
            packages_join(manifest->dir, argument);

        if (resolved == NULL ||
            !packages_dir_exists(resolved)) {

            fprintf(
                stderr,
                "rivo: la ruta '%s' no existe\n",
                argument
            );

            free(resolved);
            manifest_destroy(manifest);

            return 1;
        }

        free(resolved);

    } else {

        const char *at = strchr(argument, '@');

        if (at == NULL || at == argument ||
            at[1] == '\0') {

            fprintf(
                stderr,
                "rivo: usa `rivo add nombre@version` "
                "o `rivo add ruta/al/paquete`\n"
            );

            manifest_destroy(manifest);
            return 1;
        }

        size_t length = (size_t)(at - argument);

        if (length >= sizeof(name)) {

            fprintf(stderr, "rivo: nombre demasiado largo\n");
            manifest_destroy(manifest);
            return 1;
        }

        memcpy(name, argument, length);
        name[length] = '\0';

        value = at + 1;

        if (!semver_valid_requirement(value)) {

            fprintf(
                stderr,
                "rivo: '%s' no es una version semver valida\n",
                value
            );

            manifest_destroy(manifest);
            return 1;
        }

        char package_dir[PATH_MAX];

        if (!packages_locate_version(
                &(ResolveContext){
                    .project_dir = manifest->dir,
                    .lock = NULL,
                    .registry_url = manifest->registro_url,
                    .registry_refresh = 0,
                    .errbuf = errbuf,
                    .errlen = sizeof(errbuf)
                },
                name,
                value,
                package_dir,
                sizeof(package_dir))) {

            fprintf(
                stderr,
                "rivo: no se encontro '%s' que cumpla '%s' "
                "en .rivo/deps ni vendor\n",
                name,
                value
            );

            manifest_destroy(manifest);
            return 1;
        }
    }

    if (!packages_valid_name(name)) {

        fprintf(
            stderr,
            "rivo: nombre de dependencia no valido: '%s'\n",
            name
        );

        manifest_destroy(manifest);
        return 1;
    }

    char *toml_path = packages_manifest_path(manifest->dir);

    char *text = toml_path ? packages_read_file(toml_path) : NULL;

    if (text == NULL) {

        fprintf(
            stderr,
            "rivo: no se pudo leer '%s'\n",
            toml_path ? toml_path : "rivo.toml"
        );

        free(toml_path);
        manifest_destroy(manifest);

        return 1;
    }

    char *updated =
        packages_toml_add(
            text,
            name,
            value,
            raw_value,
            errbuf,
            sizeof(errbuf)
        );

    free(text);

    if (updated == NULL) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        free(toml_path);
        manifest_destroy(manifest);

        return 1;
    }

    int ok = packages_write_file(toml_path, updated);

    free(updated);
    free(toml_path);

    if (!ok) {

        fprintf(stderr, "rivo: no se pudo escribir rivo.toml\n");
        manifest_destroy(manifest);

        return 1;
    }

    if (raw_value)
        printf("Anadida dependencia: %s = %s\n", name, value);
    else
        printf("Anadida dependencia: %s = \"%s\"\n", name, value);

    manifest_destroy(manifest);

    return packages_command_install();
}


static int packages_command_remove(const char *name)
{
    ProjectManifest *manifest = NULL;
    char errbuf[512];

    if (!packages_load_project(
            &manifest,
            errbuf,
            sizeof(errbuf))) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        return 1;
    }

    char *toml_path = packages_manifest_path(manifest->dir);

    char *text = toml_path ? packages_read_file(toml_path) : NULL;

    if (text == NULL) {

        fprintf(stderr, "rivo: no se pudo leer rivo.toml\n");

        free(toml_path);
        manifest_destroy(manifest);

        return 1;
    }

    char *updated =
        packages_toml_remove(
            text,
            name,
            errbuf,
            sizeof(errbuf)
        );

    free(text);

    if (updated == NULL) {

        fprintf(stderr, "rivo: %s\n", errbuf);
        free(toml_path);
        manifest_destroy(manifest);

        return 1;
    }

    int ok = packages_write_file(toml_path, updated);

    free(updated);
    free(toml_path);

    if (!ok) {

        fprintf(stderr, "rivo: no se pudo escribir rivo.toml\n");
        manifest_destroy(manifest);

        return 1;
    }

    printf("Eliminada dependencia: %s\n", name);

    manifest_destroy(manifest);

    return packages_command_install();
}


int packages_is_command(const char *name)
{
    if (name == NULL)
        return 0;

    return strcmp(name, "install") == 0 ||
        strcmp(name, "update") == 0 ||
        strcmp(name, "add") == 0 ||
        strcmp(name, "remove") == 0 ||
        strcmp(name, "tree") == 0 ||
        strcmp(name, "list") == 0 ||
        strcmp(name, "search") == 0 ||
        strcmp(name, "info") == 0 ||
        strcmp(name, "publish") == 0;
}


int packages_run(int argc, char **argv)
{
    if (argc < 1 || argv == NULL || argv[0] == NULL)
        return 1;

    const char *command = argv[0];

    if (strcmp(command, "install") == 0)
        return packages_command_install_impl(0);

    if (strcmp(command, "update") == 0)
        return packages_command_install_impl(1);

    if (strcmp(command, "add") == 0) {

        if (argc < 2) {

            fprintf(
                stderr,
                "rivo: uso: rivo add nombre@version | "
                "rivo add ruta | rivo add <url-git>#<tag>\n"
            );

            return 1;
        }

        return packages_command_add(argv[1]);
    }

    if (strcmp(command, "remove") == 0) {

        if (argc < 2) {

            fprintf(
                stderr,
                "rivo: uso: rivo remove <nombre>\n"
            );

            return 1;
        }

        return packages_command_remove(argv[1]);
    }

    if (strcmp(command, "tree") == 0)
        return packages_command_tree();

    if (strcmp(command, "list") == 0)
        return packages_command_list();

    if (strcmp(command, "search") == 0) {

        if (argc < 2) {

            fprintf(
                stderr,
                "rivo: uso: rivo search <texto>\n"
            );

            return 1;
        }

        return packages_command_search(argv[1]);
    }

    if (strcmp(command, "publish") == 0) {

        if (argc < 2) {

            fprintf(
                stderr,
                "rivo: uso: rivo publish <tag>\n"
            );

            return 1;
        }

        return packages_command_publish(argv[1]);
    }

    if (strcmp(command, "info") == 0) {

        if (argc < 2) {

            fprintf(
                stderr,
                "rivo: uso: rivo info <paquete>\n"
            );

            return 1;
        }

        return packages_command_info(argv[1]);
    }

    fprintf(stderr, "rivo: subcomando desconocido: %s\n", command);

    return 1;
}
