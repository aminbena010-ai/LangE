/* =========================================================
 * MANIFIESTO DE PROYECTO (rivo.toml)
 *
 * Parser TOML deliberadamente mínimo: solo cadenas entre
 * comillas dobles, cabeceras [tabla], comentarios '#' y
 * pares clave = "valor". Es suficiente para el manifiesto
 * y evita la complejidad de un parser TOML completo.
 * ========================================================= */

#include "manifest.h"
#include "semver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#undef PATH_MAX
#define PATH_MAX 32768
#elif !defined(PATH_MAX)
#define PATH_MAX 32768
#endif

#define MANIFEST_FILENAME "rivo.toml"
#define MANIFEST_MAX_UP 32

/* Tablas reconocidas */
#define TABLE_NONE 0
#define TABLE_PROYECTO 1
#define TABLE_DEPENDENCIAS 2
#define TABLE_COMPILADOR 3
#define TABLE_REGISTRO 4


/* =========================================================
 * UTILIDADES
 * ========================================================= */

static char *manifest_string_duplicate(const char *value)
{
    if (!value)
        return NULL;

    size_t length = strlen(value);

    char *copy = malloc(length + 1);

    if (!copy)
        return NULL;

    memcpy(copy, value, length + 1);

    return copy;
}


static int manifest_file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (!file)
        return 0;

    fclose(file);

    return 1;
}


static char *manifest_join_path(
    const char *directory,
    const char *name
)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);

    char *joined =
        malloc(directory_length + name_length + 2);

    if (!joined)
        return NULL;

    memcpy(joined, directory, directory_length);
    joined[directory_length] = '/';

    memcpy(
        joined + directory_length + 1,
        name,
        name_length + 1
    );

    return joined;
}


static char *manifest_trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r')
        text++;

    size_t length = strlen(text);

    while (length > 0 &&
           (text[length - 1] == ' ' ||
            text[length - 1] == '\t' ||
            text[length - 1] == '\r'))
    {
        text[--length] = '\0';
    }

    return text;
}


/*
 * Directorio que contiene una ruta de archivo:
 *
 *   ruta/a/rivo.toml  ->  ruta/a
 *   rivo.toml         ->  .
 */
static char *manifest_parent_directory(const char *path)
{
    const char *slash = strrchr(path, '/');

#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');

    if (backslash && (!slash || backslash > slash))
        slash = backslash;
#endif

    if (!slash)
        return manifest_string_duplicate(".");

    size_t length = (size_t)(slash - path);

    char *directory = malloc(length + 1);

    if (!directory)
        return NULL;

    memcpy(directory, path, length);
    directory[length] = '\0';

    return directory;
}


/*
 * Busca rivo.toml subiendo desde start_dir.
 *
 * Se usan niveles "../" hasta MANIFEST_MAX_UP; así no
 * hace falta getcwd y funciona igual en todas las
 * plataformas.
 */
static char *manifest_find_upward(const char *start_dir)
{
    if (!start_dir || start_dir[0] == '\0')
        start_dir = ".";

    char candidate[PATH_MAX];

    for (int level = 0;
         level <= MANIFEST_MAX_UP;
         level++)
    {
        int written;

        if (level == 0) {
            written = snprintf(
                candidate,
                sizeof(candidate),
                "%s/%s",
                start_dir,
                MANIFEST_FILENAME
            );
        }
        else {
            char ups[MANIFEST_MAX_UP * 3 + 1];
            ups[0] = '\0';

            for (int i = 0; i < level; i++)
                strcat(ups, "../");

            written = snprintf(
                candidate,
                sizeof(candidate),
                "%s/%s%s",
                start_dir,
                ups,
                MANIFEST_FILENAME
            );
        }

        if (written < 0 || (size_t)written >= sizeof(candidate))
            break;

        if (manifest_file_exists(candidate))
            return manifest_string_duplicate(candidate);
    }

    return NULL;
}


/* =========================================================
 * PARSER
 * ========================================================= */

typedef struct ManifestParser {

    ProjectManifest *manifest;

    char *errbuf;
    size_t errlen;

    int table;
    int line;

    /* Claves ya vistas en la tabla actual (detecta duplicados) */
    int has_nombre;
    int has_version;
    int has_entrada;
    int has_binario;
    int has_optimizar;
    int has_depurar;
    int has_perfil;
    int has_advertencias;
    int has_registro_url;

} ManifestParser;


static int manifest_error(
    ManifestParser *parser,
    const char *message
)
{
    if (parser->errbuf && parser->errlen > 0) {

        snprintf(
            parser->errbuf,
            parser->errlen,
            "%s, línea %d: %s",
            MANIFEST_FILENAME,
            parser->line,
            message
        );
    }

    return 0;
}


static int manifest_error_key(
    ManifestParser *parser,
    const char *message,
    const char *key
)
{
    if (parser->errbuf && parser->errlen > 0) {

        snprintf(
            parser->errbuf,
            parser->errlen,
            "%s, línea %d: %s '%s'",
            MANIFEST_FILENAME,
            parser->line,
            message,
            key
        );
    }

    return 0;
}


static int manifest_valid_key(const char *key)
{
    if (key[0] == '\0')
        return 0;

    for (const char *cursor = key; *cursor; cursor++) {

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


static int manifest_set_string(
    ManifestParser *parser,
    char **target,
    int *already,
    const char *key,
    const char *value
)
{
    if (*already) {
        return manifest_error_key(
            parser,
            "clave duplicada",
            key
        );
    }

    *target = manifest_string_duplicate(value);

    if (!*target) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    *already = 1;

    return 1;
}


static int manifest_add_dep_dir(
    ManifestParser *parser,
    const char *value
)
{
    ProjectManifest *manifest = parser->manifest;

    char **grown =
        realloc(
            manifest->dep_dirs,
            (size_t)(manifest->dep_count + 1) *
                sizeof(char *)
        );

    if (!grown) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    manifest->dep_dirs = grown;

    char *copy = manifest_string_duplicate(value);

    if (!copy) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    manifest->dep_dirs[manifest->dep_count] = copy;
    manifest->dep_count++;

    return 1;
}


static ManifestDependency *manifest_find_dependency(
    ProjectManifest *manifest,
    const char *name
)
{
    for (int i = 0; i < manifest->dependency_count; i++) {

        if (strcmp(manifest->dependencies[i].name, name) == 0)
            return &manifest->dependencies[i];
    }

    return NULL;
}


static int manifest_add_dependency(
    ManifestParser *parser,
    const char *name,
    ManifestDependencyKind kind,
    const char *value
)
{
    ProjectManifest *manifest = parser->manifest;

    if (manifest_find_dependency(manifest, name) != NULL) {
        return manifest_error_key(
            parser,
            "dependencia duplicada",
            name
        );
    }

    ManifestDependency *grown =
        realloc(
            manifest->dependencies,
            (size_t)(manifest->dependency_count + 1) *
                sizeof(ManifestDependency)
        );

    if (!grown) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    manifest->dependencies = grown;

    ManifestDependency *dependency =
        &manifest->dependencies[
            manifest->dependency_count
        ];

    memset(dependency, 0, sizeof(*dependency));

    dependency->name = manifest_string_duplicate(name);

    if (!dependency->name) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    dependency->kind = kind;

    if (kind == MANIFEST_DEP_PATH) {

        dependency->path =
            manifest_string_duplicate(value);

        if (!dependency->path) {
            free(dependency->name);
            return manifest_error(
                parser,
                "memoria insuficiente"
            );
        }

    } else {

        dependency->requirement =
            manifest_string_duplicate(value);

        if (!dependency->requirement) {
            free(dependency->name);
            return manifest_error(
                parser,
                "memoria insuficiente"
            );
        }
    }

    manifest->dependency_count++;

    if (kind == MANIFEST_DEP_PATH)
        return manifest_add_dep_dir(parser, value);

    return 1;
}


static int manifest_add_git_dependency(
    ManifestParser *parser,
    const char *name,
    const char *git,
    const char *tag
)
{
    ProjectManifest *manifest = parser->manifest;

    if (manifest_find_dependency(manifest, name) != NULL) {
        return manifest_error_key(
            parser,
            "dependencia duplicada",
            name
        );
    }

    ManifestDependency *grown =
        realloc(
            manifest->dependencies,
            (size_t)(manifest->dependency_count + 1) *
                sizeof(ManifestDependency)
        );

    if (!grown) {
        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    manifest->dependencies = grown;

    ManifestDependency *dependency =
        &manifest->dependencies[
            manifest->dependency_count
        ];

    memset(dependency, 0, sizeof(*dependency));

    dependency->name = manifest_string_duplicate(name);
    dependency->git = manifest_string_duplicate(git);
    dependency->git_ref = manifest_string_duplicate(tag);

    if (!dependency->name ||
        !dependency->git ||
        !dependency->git_ref) {

        free(dependency->name);
        free(dependency->git);
        free(dependency->git_ref);

        memset(dependency, 0, sizeof(*dependency));

        return manifest_error(
            parser,
            "memoria insuficiente"
        );
    }

    dependency->kind = MANIFEST_DEP_GIT;

    manifest->dependency_count++;

    char *dep_dir = malloc(strlen(name) + 14);

    if (dep_dir == NULL)
        return 1;

    snprintf(
        dep_dir,
        strlen(name) + 14,
        ".rivo/deps/%s",
        name
    );

    int result = manifest_add_dep_dir(parser, dep_dir);

    free(dep_dir);

    return result;
}


/*
 * Extrae una cadena entre comillas dobles al principio de
 * `cursor`. Avanza `cursor` tras la comilla de cierre.
 * Devuelve una copia o NULL.
 */
static char *manifest_take_quoted(char **cursor)
{
    char *text = *cursor;

    if (text == NULL || text[0] != '"')
        return NULL;

    char *closing = strchr(text + 1, '"');

    if (closing == NULL)
        return NULL;

    *closing = '\0';

    char *copy = manifest_string_duplicate(text + 1);

    if (copy == NULL)
        return NULL;

    *cursor = closing + 1;

    return copy;
}


/*
 * Clasifica un valor suelto:
 * - ruta con separadores o que empieza por '.' -> ruta;
 * - restriccion semver valida -> version;
 * - cualquier otra cosa -> ruta (compatibilidad con
 *   manifiestos antiguos, donde "libs" era un directorio).
 */
static int manifest_classify_dependency(
    const char *value,
    ManifestDependencyKind *kind
)
{
    if (value[0] == '.' ||
        strchr(value, '/') != NULL ||
        strchr(value, '\\') != NULL) {

        *kind = MANIFEST_DEP_PATH;
        return 1;
    }

    if (semver_valid_requirement(value)) {
        *kind = MANIFEST_DEP_VERSION;
        return 1;
    }

    *kind = MANIFEST_DEP_PATH;

    return 1;
}


/*
 * Coma separadora de la tabla en linea, ignorando las comas
 * que forman parte de una restriccion entre comillas
 * (ej. { version = ">=1.0, <2.0" }).
 */
static char *manifest_find_unquoted_comma(char *text)
{
    int in_quotes = 0;

    for (char *cursor = text; *cursor; cursor++) {

        if (*cursor == '"') {

            in_quotes = !in_quotes;

        } else if (*cursor == ',' && !in_quotes) {

            return cursor;
        }
    }

    return NULL;
}


static int manifest_parse_inline_dependency(
    ManifestParser *parser,
    const char *name,
    char *value
)
{
    char *close = strrchr(value, '}');

    if (close == NULL) {
        return manifest_error(
            parser,
            "falta '}' en la dependencia"
        );
    }

    for (char *rest = close + 1; *rest; rest++) {

        if (*rest != ' ' && *rest != '\t') {

            return manifest_error_key(
                parser,
                "texto despues de '}' en",
                name
            );
        }
    }

    *close = '\0';

    char *declared_version = NULL;
    char *declared_path = NULL;
    char *declared_git = NULL;
    char *declared_ref = NULL;

    char *cursor = value + 1;

    while (cursor != NULL && *cursor != '\0') {

        char *comma = manifest_find_unquoted_comma(cursor);

        if (comma != NULL)
            *comma = '\0';

        char *piece = manifest_trim(cursor);

        if (*piece != '\0') {

            char *equal = strchr(piece, '=');

            if (equal == NULL) {
                return manifest_error_key(
                    parser,
                    "se esperaba clave = \"valor\" en",
                    name
                );
            }

            *equal = '\0';

            char *key = manifest_trim(piece);
            char *raw = manifest_trim(equal + 1);

            char *text_cursor = raw;
            char *text = manifest_take_quoted(&text_cursor);

            if (text == NULL) {
                return manifest_error_key(
                    parser,
                    "el valor de la dependencia debe ir "
                    "entre comillas en",
                    name
                );
            }

            char *after = text_cursor;

            while (*after == ' ' || *after == '\t')
                after++;

            if (*after != '\0') {
                free(text);
                return manifest_error_key(
                    parser,
                    "texto sobrante en",
                    name
                );
            }

            if (strcmp(key, "version") == 0) {

                if (declared_version != NULL) {
                    free(text);
                    return manifest_error_key(
                        parser,
                        "clave duplicada en",
                        name
                    );
                }

                declared_version = text;

            } else if (strcmp(key, "ruta") == 0 ||
                       strcmp(key, "path") == 0) {

                if (declared_path != NULL) {
                    free(text);
                    return manifest_error_key(
                        parser,
                        "clave duplicada en",
                        name
                    );
                }

                declared_path = text;

            } else if (strcmp(key, "git") == 0) {

                if (declared_git != NULL) {
                    free(text);
                    return manifest_error_key(
                        parser,
                        "clave duplicada en",
                        name
                    );
                }

                declared_git = text;

            } else if (strcmp(key, "tag") == 0 ||
                       strcmp(key, "rev") == 0) {

                if (declared_ref != NULL) {
                    free(text);
                    return manifest_error_key(
                        parser,
                        "clave duplicada en",
                        name
                    );
                }

                declared_ref = text;

            } else {

                free(text);

                return manifest_error_key(
                    parser,
                    "clave desconocida en la dependencia",
                    key
                );
            }
        }

        cursor = comma != NULL ? comma + 1 : NULL;
    }

    int kinds =
        (declared_version != NULL) +
        (declared_path != NULL) +
        (declared_git != NULL);

    if (kinds != 1) {

        free(declared_version);
        free(declared_path);
        free(declared_git);
        free(declared_ref);

        return manifest_error_key(
            parser,
            "la dependencia necesita exactamente "
            "'version', 'ruta' o 'git':",
            name
        );
    }

    if (declared_ref != NULL && declared_git == NULL) {

        free(declared_version);
        free(declared_path);
        free(declared_ref);

        return manifest_error_key(
            parser,
            "'tag' solo tiene sentido con 'git' en",
            name
        );
    }

    if (declared_git != NULL) {

        if (declared_git[0] == '\0' ||
            declared_ref == NULL ||
            declared_ref[0] == '\0') {

            free(declared_git);
            free(declared_ref);

            return manifest_error_key(
                parser,
                "la dependencia git necesita 'git' y "
                "'tag' (para builds reproducibles) en",
                name
            );
        }

        int result =
            manifest_add_git_dependency(
                parser,
                name,
                declared_git,
                declared_ref
            );

        free(declared_git);
        free(declared_ref);

        return result;
    }

    if (declared_version != NULL) {

        int valid = semver_valid_requirement(declared_version);

        if (!valid) {

            free(declared_version);

            return manifest_error_key(
                parser,
                "version semver no valida en",
                name
            );
        }

        int result = manifest_add_dependency(
            parser,
            name,
            MANIFEST_DEP_VERSION,
            declared_version
        );

        free(declared_version);

        return result;
    }

    int result = manifest_add_dependency(
        parser,
        name,
        MANIFEST_DEP_PATH,
        declared_path
    );

    free(declared_path);

    return result;
}


static int manifest_parse_dependency(
    ManifestParser *parser,
    const char *name,
    char *value
)
{
    value = manifest_trim(value);

    if (*value == '{')
        return manifest_parse_inline_dependency(
            parser,
            name,
            value
        );

    char *cursor = value;
    char *text = manifest_take_quoted(&cursor);

    if (text == NULL) {
        return manifest_error_key(
            parser,
            "el valor debe ser una cadena entre comillas "
            "dobles o una tabla { version = \"...\" } para",
            name
        );
    }

    ManifestDependencyKind kind;
    manifest_classify_dependency(text, &kind);

    int result = manifest_add_dependency(
        parser,
        name,
        kind,
        text
    );

    free(text);

    return result;
}


static int manifest_parse_table(
    ManifestParser *parser,
    char *line
)
{
    /* line apunta a '[' */
    char *end = strchr(line, ']');

    if (!end) {
        return manifest_error(
            parser,
            "falta ']' en la cabecera de tabla"
        );
    }

    *end = '\0';

    char *name = manifest_trim(line + 1);

    if (name[0] == '\0') {
        return manifest_error(
            parser,
            "cabecera de tabla vacía"
        );
    }

    if (strcmp(name, "proyecto") == 0) {
        parser->table = TABLE_PROYECTO;
        return 1;
    }

    if (strcmp(name, "dependencias") == 0) {
        parser->table = TABLE_DEPENDENCIAS;
        return 1;
    }

    if (strcmp(name, "compilador") == 0) {
        parser->table = TABLE_COMPILADOR;
        return 1;
    }

    if (strcmp(name, "registro") == 0) {
        parser->table = TABLE_REGISTRO;
        return 1;
    }

    return manifest_error_key(
        parser,
        "tabla desconocida",
        name
    );
}


static int manifest_parse_pair(
    ManifestParser *parser,
    char *line
)
{
    char *equal = strchr(line, '=');

    if (!equal) {
        return manifest_error(
            parser,
            "se esperaba clave = \"valor\""
        );
    }

    *equal = '\0';

    char *key = manifest_trim(line);
    char *value = manifest_trim(equal + 1);

    if (!manifest_valid_key(key)) {
        return manifest_error(
            parser,
            "nombre de clave no válido"
        );
    }

    /*
     * [dependencias] admite valor simple (ruta o version)
     * o tabla en linea { version = "..." } / { ruta = "..." }.
     */
    if (parser->table == TABLE_DEPENDENCIAS)
        return manifest_parse_dependency(parser, key, value);

    if (value[0] != '"') {
        return manifest_error_key(
            parser,
            "el valor debe ser una cadena entre comillas "
            "dobles para",
            key
        );
    }

    char *closing = strchr(value + 1, '"');

    if (!closing) {
        return manifest_error_key(
            parser,
            "falta la comilla de cierre de",
            key
        );
    }

    *closing = '\0';

    char *text = value + 1;

    if (text[0] == '\0') {
        return manifest_error_key(
            parser,
            "valor vacío para",
            key
        );
    }

    ProjectManifest *manifest = parser->manifest;

    if (parser->table == TABLE_PROYECTO) {

        if (strcmp(key, "nombre") == 0) {
            return manifest_set_string(
                parser,
                &manifest->nombre,
                &parser->has_nombre,
                key,
                text
            );
        }

        if (strcmp(key, "version") == 0) {
            return manifest_set_string(
                parser,
                &manifest->version,
                &parser->has_version,
                key,
                text
            );
        }

        if (strcmp(key, "entrada") == 0) {
            return manifest_set_string(
                parser,
                &manifest->entrada,
                &parser->has_entrada,
                key,
                text
            );
        }

        if (strcmp(key, "binario") == 0) {
            return manifest_set_string(
                parser,
                &manifest->binario,
                &parser->has_binario,
                key,
                text
            );
        }

        return manifest_error_key(
            parser,
            "clave desconocida",
            key
        );
    }

    if (parser->table == TABLE_REGISTRO) {

        if (strcmp(key, "url") == 0) {
            return manifest_set_string(
                parser,
                &manifest->registro_url,
                &parser->has_registro_url,
                key,
                text
            );
        }

        return manifest_error_key(
            parser,
            "clave desconocida",
            key
        );
    }

    if (parser->table == TABLE_COMPILADOR) {

        if (strcmp(key, "optimizar") == 0) {
            return manifest_set_string(
                parser,
                &manifest->optimizar,
                &parser->has_optimizar,
                key,
                text
            );
        }

        if (strcmp(key, "depurar") == 0) {
            return manifest_set_string(
                parser,
                &manifest->depurar,
                &parser->has_depurar,
                key,
                text
            );
        }

        if (strcmp(key, "perfil") == 0) {
            return manifest_set_string(
                parser,
                &manifest->perfil,
                &parser->has_perfil,
                key,
                text
            );
        }

        if (strcmp(key, "advertencias_como_errores") == 0) {
            return manifest_set_string(
                parser,
                &manifest->advertencias_como_errores,
                &parser->has_advertencias,
                key,
                text
            );
        }

        return manifest_error_key(
            parser,
            "clave desconocida",
            key
        );
    }

    return manifest_error(
        parser,
        "clave fuera de una tabla "
        "(falta una cabecera como [proyecto])"
    );
}


static int manifest_parse_line(
    ManifestParser *parser,
    char *line
)
{
    char *text = manifest_trim(line);

    if (text[0] == '\0' || text[0] == '#')
        return 1;

    if (text[0] == '[')
        return manifest_parse_table(parser, text);

    return manifest_parse_pair(parser, text);
}


static int manifest_parse_text(
    ProjectManifest *manifest,
    char *text,
    char *errbuf,
    size_t errlen
)
{
    ManifestParser parser;

    memset(&parser, 0, sizeof(parser));

    parser.manifest = manifest;
    parser.errbuf = errbuf;
    parser.errlen = errlen;
    parser.table = TABLE_NONE;
    parser.line = 1;

    char *cursor = text;

    while (cursor && *cursor) {

        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            newline++;
        }

        if (!manifest_parse_line(&parser, cursor))
            return 0;

        cursor = newline;
        parser.line++;
    }

    return 1;
}


/* =========================================================
 * CARGA
 * ========================================================= */

void manifest_destroy(ProjectManifest *manifest)
{
    if (!manifest)
        return;

    free(manifest->nombre);
    free(manifest->version);
    free(manifest->entrada);
    free(manifest->binario);

    for (int i = 0; i < manifest->dep_count; i++)
        free(manifest->dep_dirs[i]);

    free(manifest->dep_dirs);

    for (int i = 0; i < manifest->dependency_count; i++) {

        free(manifest->dependencies[i].name);
        free(manifest->dependencies[i].path);
        free(manifest->dependencies[i].requirement);
        free(manifest->dependencies[i].git);
        free(manifest->dependencies[i].git_ref);
    }

    free(manifest->dependencies);

    free(manifest->registro_url);
    free(manifest->optimizar);
    free(manifest->depurar);
    free(manifest->perfil);
    free(manifest->advertencias_como_errores);

    free(manifest->ruta);
    free(manifest->dir);

    free(manifest);
}


ManifestStatus manifest_load(
    const char *start_dir,
    ProjectManifest **out,
    char *errbuf,
    size_t errlen
)
{
    if (out)
        *out = NULL;

    if (errbuf && errlen > 0)
        errbuf[0] = '\0';

    char *ruta =
        manifest_find_upward(start_dir);

    if (!ruta)
        return MANIFEST_NOT_FOUND;

    FILE *file = fopen(ruta, "rb");

    if (!file) {

        if (errbuf && errlen > 0) {

            snprintf(
                errbuf,
                errlen,
                "no se pudo leer '%s'",
                ruta
            );
        }

        free(ruta);
        return MANIFEST_INVALID;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        free(ruta);
        return MANIFEST_INVALID;
    }

    long size = ftell(file);

    if (size < 0) {
        fclose(file);
        free(ruta);
        return MANIFEST_INVALID;
    }

    rewind(file);

    char *buffer = malloc((size_t)size + 1);

    if (!buffer) {
        fclose(file);
        free(ruta);
        return MANIFEST_INVALID;
    }

    size_t read =
        fread(buffer, 1, (size_t)size, file);

    fclose(file);

    buffer[read] = '\0';

    ProjectManifest *manifest =
        calloc(1, sizeof(ProjectManifest));

    if (!manifest) {
        free(buffer);
        free(ruta);
        return MANIFEST_INVALID;
    }

    manifest->ruta = ruta;

    manifest->dir =
        manifest_parent_directory(ruta);

    if (!manifest->dir) {
        free(buffer);
        manifest_destroy(manifest);
        return MANIFEST_INVALID;
    }

    int ok = manifest_parse_text(
        manifest,
        buffer,
        errbuf,
        errlen
    );

    free(buffer);

    if (!ok) {
        manifest_destroy(manifest);
        return MANIFEST_INVALID;
    }

    if (out)
        *out = manifest;
    else
        manifest_destroy(manifest);

    return MANIFEST_OK;
}
