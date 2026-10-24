/* =========================================================
 * SISTEMA DE BUILD (estilo Cargo)
 *
 * Cuando hay un manifiesto rivo.toml, los artefactos van a
 * target/<perfil>/ dentro del proyecto: debug o release.
 *
 * Además gestiona:
 *   - caché por fecha de modificación (.fuentes): si nada
 *     cambió, se reutiliza el binario sin recompilar;
 *   - estado de compilación con colores en stderr, al
 *     estilo de Cargo (respeta NO_COLOR y los pipes);
 *   - tiempos por fase para el modo -v.
 * ========================================================= */

#ifndef RIVO_BUILD_H
#define RIVO_BUILD_H

#include <stddef.h>

typedef enum BuildProfile {

    BUILD_PROFILE_DEBUG = 0,
    BUILD_PROFILE_RELEASE = 1

} BuildProfile;

typedef struct BuildPaths {

    /* <dir del manifiesto>/target/<perfil> */
    char *dir;

    /* Ejecutable: <dir>/<binario> */
    char *binary;

    /* Objeto: <dir>/<binario>.o */
    char *object;

    /* Caché de fuentes: <dir>/<binario>.fuentes */
    char *sources;

    /* Huella de compilación: <dir>/<binario>.huella */
    char *fingerprint;

} BuildPaths;

const char *build_profile_name(BuildProfile profile);

/* release si optimize >= 2, debug en caso contrario */
BuildProfile build_profile_from_optimize(int optimize);

/*
 * Crea el directorio y todos sus padres (mkdir -p).
 * 1 = ok, 0 = fallo.
 */
int build_ensure_dir(const char *dir);

BuildPaths *build_paths_create(
    const char *manifest_dir,
    BuildProfile profile,
    const char *binary_name
);

void build_paths_destroy(BuildPaths *paths);

/*
 * Caché por huella: 1 si el binario se puede reutilizar.
 *
 * Camino rápido: si ninguna fuente es más nueva que la
 * huella (.huella), está fresco. Si algo cambió de fecha,
 * se recalculan los SHA-256 y solo se recompila cuando el
 * contenido (o la versión/flags) difiere de verdad.
 */
int build_cache_is_fresh(
    const BuildPaths *paths,
    const char *manifest_path,
    const char *input_file,
    int optimize
);

/*
 * Guarda la lista de fuentes (.ro + rivo.toml) usadas en
 * el último build y la huella SHA-256 de la compilación.
 */
int build_cache_write(
    const BuildPaths *paths,
    const char *manifest_path,
    const char *input_file,
    char **module_sources,
    int module_source_count,
    int optimize
);


/* =========================================================
 * ESTILO DE COMPILACIÓN (stderr, con color si hay tty)
 * ========================================================= */

/* 1 si la salida de estado puede llevar color */
int build_status_enabled(void);

void build_status_compiling(
    const char *nombre,
    const char *version,
    const char *dir
);

void build_status_linking(const char *profile_name);

void build_status_finished(
    const char *profile_name,
    double seconds,
    int cached
);

void build_status_running(const char *binary);


/* =========================================================
 * TIEMPOS
 * ========================================================= */

/* Segundos desde un punto arbitrario (para duraciones) */
double build_now(void);

/*
 * Imprime "[rivo] <fase>: <ms> ms" por stdout.
 * El llamador decide (verbose); build no lo comprueba.
 */
void build_phase_timing(const char *phase, double from, double to);

#endif /* RIVO_BUILD_H */
