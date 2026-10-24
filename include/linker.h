#ifndef LINKER_H
#define LINKER_H

#include "error.h"

/* =========================================================
 * TIPOS DE LINKER
 * ========================================================= */

typedef enum {
    LINKER_PLATFORM_WINDOWS,
    LINKER_PLATFORM_LINUX,
    LINKER_PLATFORM_MACOS
} LinkerPlatform;

typedef enum {
    LINKER_ARCH_X86_64,
    LINKER_ARCH_ARM64
} LinkerArchitecture;


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

typedef struct LinkerConfig {

    /* Plataforma destino */
    LinkerPlatform platform;

    /* Arquitectura destino */
    LinkerArchitecture architecture;

    /* Archivo objeto de entrada */
    const char *object_file;

    /* Ejecutable de salida */
    const char *output_file;

    /* Librerías adicionales */
    const char **libraries;
    int library_count;
    int library_capacity;

    /* Directorios donde buscar librerías */
    const char **library_paths;
    int library_path_count;
    int library_path_capacity;

    /* Directorios de inclusión */
    const char **include_paths;
    int include_path_count;
    int include_path_capacity;

    /* Generar información de depuración */
    int debug_info;

    /* Ejecutar linker con optimización */
    int optimize;

    /* Usar runtime de nuestro lenguaje */
    int use_runtime;

    /* Ruta del runtime */
    const char *runtime_path;

} LinkerConfig;


/* =========================================================
 * LINKER
 * ========================================================= */

typedef struct Linker {

    /* Configuración */
    LinkerConfig config;

    /* Sistema de errores */
    ErrorContext *errors;

    /* Estado */
    int had_error;

    /* Comando generado */
    char *command;

} Linker;


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

Linker *linker_create(
    const LinkerConfig *config,
    ErrorContext *errors
);

void linker_destroy(Linker *linker);


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

LinkerConfig linker_default_config(void);

/*
 * Compilador C/driver que usa el linker (mismo nombre
 * para compilar los ficheros C externos).
 */
const char *linker_compiler(void);

int linker_set_platform(
    Linker *linker,
    LinkerPlatform platform
);

int linker_set_architecture(
    Linker *linker,
    LinkerArchitecture architecture
);

int linker_set_object_file(
    Linker *linker,
    const char *filename
);

int linker_set_output_file(
    Linker *linker,
    const char *filename
);

int linker_add_library(
    Linker *linker,
    const char *library
);

int linker_add_library_path(
    Linker *linker,
    const char *path
);

int linker_add_include_path(
    Linker *linker,
    const char *path
);

int linker_set_runtime(
    Linker *linker,
    const char *runtime_path
);


/* =========================================================
 * LINK
 * ========================================================= */

/*
 * Genera el ejecutable final a partir del archivo objeto.
 */
int linker_link(Linker *linker);


/*
 * Construye el comando del linker sin ejecutarlo.
 */
int linker_build_command(Linker *linker);


/*
 * Ejecuta el comando generado.
 */
int linker_execute(Linker *linker);


/*
 * Comprueba si el linker está disponible.
 */
int linker_check_available(Linker *linker);


/* =========================================================
 * INFORMACIÓN
 * ========================================================= */

const char *linker_platform_name(LinkerPlatform platform);

const char *linker_architecture_name(
    LinkerArchitecture architecture
);

const char *linker_get_command(
    const Linker *linker
);

int linker_has_error(
    const Linker *linker
);

#endif