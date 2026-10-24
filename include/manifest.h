/* =========================================================
 * MANIFIESTO DE PROYECTO (rivo.toml)
 *
 * Archivo opcional, tipo Cargo.toml, que describe el
 * proyecto: nombre, versión, archivo de entrada, rutas de
 * dependencias y opciones por defecto del compilador.
 *
 * El compilador lo busca subiendo desde el directorio del
 * archivo de entrada (o desde el directorio de trabajo).
 * Si no existe, todo funciona igual que antes.
 * ========================================================= */

#ifndef RIVO_MANIFEST_H
#define RIVO_MANIFEST_H

#include <stddef.h>

typedef enum ManifestStatus {

    /* Manifiesto cargado y válido */
    MANIFEST_OK = 0,

    /* No hay rivo.toml en los directorios buscados */
    MANIFEST_NOT_FOUND = 1,

    /* Hay un rivo.toml pero no se pudo interpretar */
    MANIFEST_INVALID = 2

} ManifestStatus;

typedef enum ManifestDependencyKind {

    /* nombre = "../ruta" o { ruta = "..." } */
    MANIFEST_DEP_PATH = 0,

    /* nombre = "1.2.0" o { version = "..." } */
    MANIFEST_DEP_VERSION = 1,

    /* { git = "url", tag = "v1.0.0" } */
    MANIFEST_DEP_GIT = 2

} ManifestDependencyKind;

typedef struct ManifestDependency {

    char *name;

    ManifestDependencyKind kind;

    /* Ruta relativa al manifiesto (solo DEP_PATH) */
    char *path;

    /* Restriccion semver, ej. "^1.2" (solo DEP_VERSION) */
    char *requirement;

    /* URL del repositorio y tag fijado (solo DEP_GIT) */
    char *git;
    char *git_ref;

} ManifestDependency;

typedef struct ProjectManifest {

    /* [proyecto] */
    char *nombre;
    char *version;
    char *entrada;
    char *binario;

    /* [dependencias] con nombre, tipo y valor */
    ManifestDependency *dependencies;
    int dependency_count;

    /* Rutas de dependencias locales (derivado) */
    char **dep_dirs;
    int dep_count;

    /* [registro] url del indice Git (opcional) */
    char *registro_url;

    /* [compilador]: valores como están en el archivo, o NULL */
    char *optimizar;
    char *depurar;
    char *perfil;                   /* "debug" | "release" */
    char *advertencias_como_errores; /* "true" | "false" */

    /* Ruta del rivo.toml cargado y directorio que lo contiene */
    char *ruta;
    char *dir;

} ProjectManifest;

/*
 * Busca rivo.toml subiendo desde start_dir y lo carga.
 *
 * - MANIFEST_OK:        *out apunta al manifiesto (liberar con
 *                       manifest_destroy).
 * - MANIFEST_NOT_FOUND: no hay manifiesto; *out queda NULL.
 * - MANIFEST_INVALID:   errbuf recibe el mensaje de error.
 */
ManifestStatus manifest_load(
    const char *start_dir,
    ProjectManifest **out,
    char *errbuf,
    size_t errlen
);

void manifest_destroy(ProjectManifest *manifest);

#endif /* RIVO_MANIFEST_H */
