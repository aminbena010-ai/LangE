/* =========================================================
 * SISTEMA DE BUILD (estilo Cargo)
 * ========================================================= */

#include "build.h"
#include "hash.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define RIVO_MKDIR(path) _mkdir(path)
#define RIVO_ISATTY(fd) _isatty(fd)
#define RIVO_STAT_STRUCT struct _stat
#define RIVO_STAT(path, buf) _stat(path, buf)
#else
#include <unistd.h>
#define RIVO_MKDIR(path) mkdir(path, 0777)
#define RIVO_ISATTY(fd) isatty(fd)
#define RIVO_STAT_STRUCT struct stat
#define RIVO_STAT(path, buf) stat(path, buf)
#endif

#if defined(_WIN32)
#undef PATH_MAX
#define PATH_MAX 32768
#elif !defined(PATH_MAX)
#define PATH_MAX 32768
#endif

#define BUILD_COLOR_BOLD_CYAN "\033[1;36m"
#define BUILD_COLOR_BOLD_GREEN "\033[1;32m"
#define BUILD_COLOR_BOLD "\033[1m"
#define BUILD_COLOR_RESET "\033[0m"


/* =========================================================
 * UTILIDADES
 * ========================================================= */

static char *build_string_duplicate(const char *value)
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


static char *build_join_path(
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


/*
 * Fecha de última modificación en segundos.
 * 0 = el archivo no existe.
 */
static double build_file_mtime(const char *path)
{
    RIVO_STAT_STRUCT info;

    if (RIVO_STAT(path, &info) != 0)
        return 0.0;

    return (double)info.st_mtime;
}


const char *build_profile_name(BuildProfile profile)
{
    return profile == BUILD_PROFILE_RELEASE
        ? "release"
        : "debug";
}


BuildProfile build_profile_from_optimize(int optimize)
{
    return optimize >= 2
        ? BUILD_PROFILE_RELEASE
        : BUILD_PROFILE_DEBUG;
}


/* =========================================================
 * DIRECTORIOS
 * ========================================================= */

int build_ensure_dir(const char *dir)
{
    if (!dir || dir[0] == '\0')
        return 0;

    char buffer[PATH_MAX];

    size_t length = strlen(dir);

    if (length >= sizeof(buffer))
        return 0;

    memcpy(buffer, dir, length + 1);

    /*
     * Crear cada componente del camino: a/b/c -> a, a/b,
     * a/b/c. Los separadores existentes se respetan.
     */
    for (size_t i = 1; i <= length; i++) {

        if (buffer[i] != '/' && buffer[i] != '\0')
            continue;

        char saved = buffer[i];
        buffer[i] = '\0';

        if (buffer[0] != '\0') {

            if (RIVO_MKDIR(buffer) != 0) {

                RIVO_STAT_STRUCT info;

                if (RIVO_STAT(buffer, &info) != 0)
                    return 0;
            }
        }

        buffer[i] = saved;
    }

    return 1;
}


/* =========================================================
 * RUTAS DE ARTEFACTOS
 * ========================================================= */

BuildPaths *build_paths_create(
    const char *manifest_dir,
    BuildProfile profile,
    const char *binary_name
)
{
    if (!manifest_dir || !binary_name)
        return NULL;

    BuildPaths *paths =
        calloc(1, sizeof(BuildPaths));

    if (!paths)
        return NULL;

    char *target_dir =
        build_join_path(manifest_dir, "target");

    if (!target_dir) {
        build_paths_destroy(paths);
        return NULL;
    }

    char *profile_dir =
        build_join_path(target_dir, build_profile_name(profile));

    free(target_dir);

    if (!profile_dir) {
        build_paths_destroy(paths);
        return NULL;
    }

    paths->dir = profile_dir;

    paths->binary =
        build_join_path(profile_dir, binary_name);

    char *object_name =
        malloc(strlen(binary_name) + 3);

    if (object_name) {
        sprintf(object_name, "%s.o", binary_name);
        paths->object =
            build_join_path(profile_dir, object_name);
        free(object_name);
    }

    char *sources_name =
        malloc(strlen(binary_name) + 9);

    if (sources_name) {
        sprintf(sources_name, "%s.fuentes", binary_name);
        paths->sources =
            build_join_path(profile_dir, sources_name);
        free(sources_name);
    }

    char *fingerprint_name =
        malloc(strlen(binary_name) + 9);

    if (fingerprint_name) {
        sprintf(fingerprint_name, "%s.huella", binary_name);
        paths->fingerprint =
            build_join_path(profile_dir, fingerprint_name);
        free(fingerprint_name);
    }

    if (!paths->binary || !paths->object || !paths->sources ||
        !paths->fingerprint) {
        build_paths_destroy(paths);
        return NULL;
    }

    return paths;
}


void build_paths_destroy(BuildPaths *paths)
{
    if (!paths)
        return;

    free(paths->dir);
    free(paths->binary);
    free(paths->object);
    free(paths->sources);
    free(paths->fingerprint);

    free(paths);
}


/* =========================================================
 * CACHÉ POR HUELLA (mtime + SHA-256)
 * ========================================================= */

/*
 * Comprueba que el archivo exista y no sea más reciente
 * que la referencia. Devuelve 0 si hay que recompilar.
 */
static int build_source_is_older(
    const char *path,
    double reference_mtime
)
{
    double mtime =
        build_file_mtime(path);

    if (mtime <= 0.0)
        return 0;

    /*
     * Margen de 1 s: la resolución de st_mtime es de
     * segundos, así que un archivo tocado en el mismo
     * segundo que la huella debe ir al camino lento
     * (comparación de hashes), no darse por fresco.
     */
    if (mtime + 1.0 > reference_mtime)
        return 0;

    return 1;
}


typedef struct BuildBuffer {
    char *data;
    size_t length;
    size_t capacity;
} BuildBuffer;

static int build_buffer_append(
    BuildBuffer *buffer,
    const char *text
)
{
    size_t add = strlen(text);
    size_t needed = buffer->length + add + 1;

    if (needed > buffer->capacity) {
        size_t next = buffer->capacity ? buffer->capacity : 256;

        while (next < needed)
            next *= 2;

        char *grown = realloc(buffer->data, next);

        if (!grown)
            return 0;

        buffer->data = grown;
        buffer->capacity = next;
    }

    memcpy(buffer->data + buffer->length, text, add + 1);
    buffer->length += add;

    return 1;
}


static int build_buffer_append_file_hash(
    BuildBuffer *buffer,
    const char *label,
    const char *path
)
{
    if (!path)
        return 1;

    unsigned char raw[32];
    char digest[65];

    if (hash_file_sha256(path, raw))
        hash_hex(raw, digest);
    else
        strcpy(digest, "-");

    size_t line_size = strlen(label) + strlen(digest) +
        strlen(path) + 4;

    char *line = malloc(line_size);

    if (!line)
        return 0;

    snprintf(line, line_size, "%s %s %s\n", label, digest, path);

    int ok = build_buffer_append(buffer, line);

    free(line);

    return ok;
}


/*
 * Texto canónico de la huella: versión, flags y SHA-256 de
 * cada fuente (manifiesto, entrada y módulos) más el
 * runtime enlazado. Si cambia cualquier cosa, cambia.
 */
static char *build_fingerprint_text(
    char **sources,
    int source_count,
    int optimize
)
{
    BuildBuffer buffer = {0};

    char header[256];

    snprintf(
        header,
        sizeof(header),
        "version=%s\noptimize=%d\nperfil=%s\n",
        RIVO_VERSION_FULL,
        optimize,
        build_profile_name(
            build_profile_from_optimize(optimize))
    );

    if (!build_buffer_append(&buffer, header)) {
        free(buffer.data);
        return NULL;
    }

    for (int i = 0; i < source_count; i++) {
        if (!build_buffer_append_file_hash(
                &buffer, "fuente", sources[i])) {
            free(buffer.data);
            return NULL;
        }
    }

#ifdef RIVO_RUNTIME_PATH
    if (!build_buffer_append_file_hash(
            &buffer, "runtime", RIVO_RUNTIME_PATH)) {
        free(buffer.data);
        return NULL;
    }
#else
    if (!build_buffer_append(&buffer, "runtime=-\n")) {
        free(buffer.data);
        return NULL;
    }
#endif

    if (!buffer.data) {
        buffer.data = build_string_duplicate("");
    }

    return buffer.data;
}


static char **build_sources_read(
    const BuildPaths *paths,
    int *out_count
)
{
    *out_count = 0;

    FILE *file = fopen(paths->sources, "rb");

    if (!file)
        return NULL;

    char **items = NULL;
    int count = 0;
    int capacity = 0;
    char line[PATH_MAX];

    while (fgets(line, sizeof(line), file)) {

        size_t length = strlen(line);

        while (length > 0 &&
               (line[length - 1] == '\n' ||
                line[length - 1] == '\r'))
        {
            line[--length] = '\0';
        }

        if (length == 0)
            continue;

        if (count == capacity) {
            int next = capacity ? capacity * 2 : 8;

            char **grown = realloc(
                items, (size_t)next * sizeof(char *));

            if (!grown)
                break;

            items = grown;
            capacity = next;
        }

        items[count] = build_string_duplicate(line);

        if (!items[count])
            break;

        count++;
    }

    fclose(file);

    *out_count = count;

    return items;
}


static void build_sources_free(char **items, int count)
{
    if (!items)
        return;

    for (int i = 0; i < count; i++)
        free(items[i]);

    free(items);
}


static char *build_file_read_all(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (!file)
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

    char *data = malloc((size_t)size + 1);

    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t got = fread(data, 1, (size_t)size, file);

    fclose(file);

    if (got != (size_t)size) {
        free(data);
        return NULL;
    }

    data[size] = '\0';

    return data;
}


int build_cache_is_fresh(
    const BuildPaths *paths,
    const char *manifest_path,
    const char *input_file,
    int optimize
)
{
    (void)manifest_path;
    (void)input_file;

    if (!paths)
        return 0;

    if (build_file_mtime(paths->binary) <= 0.0)
        return 0;

    double fingerprint_mtime =
        build_file_mtime(paths->fingerprint);

    if (fingerprint_mtime <= 0.0)
        return 0;

    /*
     * La cabecera (versión, nivel -O y perfil) se compara
     * siempre: cambiar flags debe invalidar aunque ninguna
     * fuente haya cambiado de fecha.
     */
    char *stored = build_file_read_all(paths->fingerprint);

    if (!stored)
        return 0;

    char header[256];

    snprintf(
        header,
        sizeof(header),
        "version=%s\noptimize=%d\nperfil=%s\n",
        RIVO_VERSION_FULL,
        optimize,
        build_profile_name(
            build_profile_from_optimize(optimize))
    );

    if (strncmp(stored, header, strlen(header)) != 0) {
        free(stored);
        return 0;
    }

    free(stored);

    int source_count = 0;

    char **sources =
        build_sources_read(paths, &source_count);

    if (!sources || source_count == 0) {
        build_sources_free(sources, source_count);
        return 0;
    }

    /*
     * Camino rápido: si nada es más nuevo que la huella,
     * no hace falta leer ni hashear contenidos.
     */
    int fast = 1;

    for (int i = 0; i < source_count; i++) {
        if (!build_source_is_older(
                sources[i], fingerprint_mtime)) {
            fast = 0;
            break;
        }
    }

    if (fast) {
        build_sources_free(sources, source_count);
        return 1;
    }

    /*
     * Camino lento: comparar contenido. Un touch sin
     * cambios reales sigue siendo fresco.
     */
    char *expected = build_fingerprint_text(
        sources, source_count, optimize);

    stored = build_file_read_all(paths->fingerprint);

    int fresh =
        expected != NULL &&
        stored != NULL &&
        strcmp(expected, stored) == 0;

    free(expected);
    free(stored);
    build_sources_free(sources, source_count);

    return fresh;
}


int build_cache_write(
    const BuildPaths *paths,
    const char *manifest_path,
    const char *input_file,
    char **module_sources,
    int module_source_count,
    int optimize
)
{
    if (!paths)
        return 0;

    int total =
        module_source_count +
        (manifest_path ? 1 : 0) +
        (input_file ? 1 : 0);

    char **combined =
        calloc(total > 0 ? (size_t)total : 1, sizeof(char *));

    if (!combined)
        return 0;

    int index = 0;

    if (manifest_path)
        combined[index++] = (char *)manifest_path;

    if (input_file)
        combined[index++] = (char *)input_file;

    for (int i = 0; i < module_source_count; i++)
        combined[index++] = module_sources[i];

    FILE *file = fopen(paths->sources, "wb");

    if (!file) {
        free(combined);
        return 0;
    }

    for (int i = 0; i < index; i++) {
        if (combined[i])
            fprintf(file, "%s\n", combined[i]);
    }

    fclose(file);

    int ok = 1;

    char *text = build_fingerprint_text(
        combined, index, optimize);

    if (!text) {
        free(combined);
        return 0;
    }

    FILE *fingerprint =
        fopen(paths->fingerprint, "wb");

    if (!fingerprint) {
        free(text);
        free(combined);
        return 0;
    }

    if (fwrite(text, 1, strlen(text), fingerprint) !=
        strlen(text))
    {
        ok = 0;
    }

    fclose(fingerprint);

    free(text);
    free(combined);

    return ok;
}


/* =========================================================
 * ESTILO DE COMPILACIÓN
 * ========================================================= */

/*
 * Color solo en una terminal real y sin NO_COLOR. La
 * salida va por stderr: stdout queda limpia para --run.
 */
int build_status_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {

        const char *no_color =
            getenv("NO_COLOR");

        cached =
            RIVO_ISATTY(2) &&
            (no_color == NULL || no_color[0] == '\0');
    }

    return cached;
}


void build_status_compiling(
    const char *nombre,
    const char *version,
    const char *dir
)
{
    if (build_status_enabled()) {

        fprintf(
            stderr,
            BUILD_COLOR_BOLD_CYAN "   Compilando "
            BUILD_COLOR_RESET
            BUILD_COLOR_BOLD "%s%s%s" BUILD_COLOR_RESET
            " (%s)\n",
            nombre ? nombre : "proyecto",
            version ? " " : "",
            version ? version : "",
            dir ? dir : "."
        );
    }
    else {

        fprintf(
            stderr,
            "   Compilando %s%s%s (%s)\n",
            nombre ? nombre : "proyecto",
            version ? " " : "",
            version ? version : "",
            dir ? dir : "."
        );
    }
}


void build_status_linking(const char *profile_name)
{
    if (build_status_enabled()) {

        fprintf(
            stderr,
            BUILD_COLOR_BOLD_CYAN "    Enlazando "
            BUILD_COLOR_RESET
            BUILD_COLOR_BOLD "%s" BUILD_COLOR_RESET "\n",
            profile_name ? profile_name : "debug"
        );
    }
    else {

        fprintf(
            stderr,
            "    Enlazando %s\n",
            profile_name ? profile_name : "debug"
        );
    }
}


void build_status_finished(
    const char *profile_name,
    double seconds,
    int cached
)
{
    if (build_status_enabled()) {

        fprintf(
            stderr,
            BUILD_COLOR_BOLD_GREEN "    Finished" BUILD_COLOR_RESET
            " %s%s%s en %.2fs\n",
            profile_name ? profile_name : "debug",
            cached ? " " : "",
            cached ? "(sin cambios)" : "",
            seconds
        );
    }
    else {

        fprintf(
            stderr,
            "    Finished %s%s%s en %.2fs\n",
            profile_name ? profile_name : "debug",
            cached ? " " : "",
            cached ? "(sin cambios)" : "",
            seconds
        );
    }
}


void build_status_running(const char *binary)
{
    if (build_status_enabled()) {

        fprintf(
            stderr,
            BUILD_COLOR_BOLD_GREEN "     Ejecutando" BUILD_COLOR_RESET
            " %s\n",
            binary ? binary : ""
        );
    }
    else {

        fprintf(
            stderr,
            "     Ejecutando %s\n",
            binary ? binary : ""
        );
    }
}


/* =========================================================
 * TIEMPOS
 * ========================================================= */

double build_now(void)
{
    struct timespec now;

    timespec_get(&now, TIME_UTC);

    return (double)now.tv_sec +
           (double)now.tv_nsec / 1000000000.0;
}


void build_phase_timing(
    const char *phase,
    double from,
    double to
)
{
    double milliseconds =
        (to - from) * 1000.0;

    printf(
        "[rivo] %s: %.1f ms\n",
        phase ? phase : "fase",
        milliseconds
    );
}
