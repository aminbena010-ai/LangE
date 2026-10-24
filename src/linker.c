#include "linker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define RIVO_ACCESS_X(path) _access(path, 0)
#else
#include <unistd.h>
#define RIVO_ACCESS_X(path) access(path, X_OK)
#endif


/* =========================================================
 * CONSTANTES
 * ========================================================= */

#define LINKER_INITIAL_CAPACITY 8
#define LINKER_COMMAND_INITIAL_SIZE 1024


/* =========================================================
 * UTILIDADES INTERNAS
 * ========================================================= */

static char *linker_strdup(const char *string)
{
    if (!string)
        return NULL;

    size_t length = strlen(string);

    char *copy = malloc(length + 1);

    if (!copy)
        return NULL;

    memcpy(copy, string, length + 1);

    return copy;
}


static int linker_string_array_add(
    const char ***array,
    int *count,
    int *capacity,
    const char *value
)
{
    if (!array || !count || !capacity || !value)
        return 0;

    if (*count >= *capacity)
    {
        int new_capacity =
            (*capacity == 0)
                ? LINKER_INITIAL_CAPACITY
                : (*capacity * 2);

        const char **new_array =
            realloc(
                (void *)*array,
                sizeof(char *) * new_capacity
            );

        if (!new_array)
            return 0;

        *array = new_array;
        *capacity = new_capacity;
    }

    (*array)[*count] = linker_strdup(value);

    if (!(*array)[*count])
        return 0;

    (*count)++;

    return 1;
}


static void linker_string_array_destroy(
    const char **array,
    int count
)
{
    if (!array)
        return;

    for (int i = 0; i < count; i++)
        free((void *)array[i]);

    free((void *)array);
}


static void linker_report_error(
    Linker *linker,
    ErrorKind kind,
    const char *message
)
{
    if (!linker)
        return;

    linker->had_error = 1;

    if (linker->errors)
    {
        error_report_simple(
            linker->errors,
            kind,
            ERROR_PHASE_CODEGEN,
            0,
            0,
            message
        );
    }
}


static void linker_report_errorf(
    Linker *linker,
    ErrorKind kind,
    const char *format,
    ...
)
{
    if (!linker)
        return;

    linker->had_error = 1;

    char buffer[2048];

    va_list args;
    va_start(args, format);

    vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        args
    );

    va_end(args);

    if (linker->errors)
    {
        error_report_simple(
            linker->errors,
            kind,
            ERROR_PHASE_CODEGEN,
            0,
            0,
            buffer
        );
    }
}


/*
 * Añade texto al comando dinámico.
 */
static int linker_command_append(
    char **command,
    size_t *length,
    size_t *capacity,
    const char *text
)
{
    if (!command || !length || !capacity || !text)
        return 0;

    size_t text_length = strlen(text);

    if (*length + text_length + 1 > *capacity)
    {
        size_t new_capacity = *capacity;

        while (*length + text_length + 1 > new_capacity)
            new_capacity *= 2;

        char *new_command =
            realloc(*command, new_capacity);

        if (!new_command)
            return 0;

        *command = new_command;
        *capacity = new_capacity;
    }

    memcpy(
        *command + *length,
        text,
        text_length
    );

    *length += text_length;

    (*command)[*length] = '\0';

    return 1;
}


/*
 * Añade una cadena separada por un espacio.
 */
static int linker_command_append_arg(
    char **command,
    size_t *length,
    size_t *capacity,
    const char *argument
)
{
    if (*length > 0)
    {
        if (!linker_command_append(
                command,
                length,
                capacity,
                " "))
        {
            return 0;
        }
    }

    return linker_command_append(
        command,
        length,
        capacity,
        argument
    );
}


/*
 * Escapa argumentos para poder pasarlos al shell.

 * Unix:
 *   "archivo con espacios.o"
 *
 * Windows:
 *   "archivo con espacios.obj"
 */
static char *linker_quote_argument(
    const char *argument
)
{
    if (!argument)
        return NULL;

    size_t length = strlen(argument);

    /*
     * Caso simple:
     * no necesitamos comillas.
     */
    int needs_quotes = 0;

    for (size_t i = 0; i < length; i++)
    {
        if (isspace((unsigned char)argument[i]) ||
            argument[i] == '"' ||
            argument[i] == '\'' ||
            argument[i] == '&' ||
            argument[i] == '|' ||
            argument[i] == '<' ||
            argument[i] == '>' ||
            argument[i] == '(' ||
            argument[i] == ')' ||
            argument[i] == ';')
        {
            needs_quotes = 1;
            break;
        }
    }

    if (!needs_quotes)
        return linker_strdup(argument);

    /*
     * Usamos comillas dobles.
     *
     * Para nuestro caso de uso, las rutas generadas
     * por el compilador normalmente no contienen comillas.
     */
    size_t output_size =
        length * 2 + 3;

    char *output =
        malloc(output_size);

    if (!output)
        return NULL;

    size_t position = 0;

    output[position++] = '"';

    for (size_t i = 0; i < length; i++)
    {
        char c = argument[i];

        if (c == '"')
        {
            output[position++] = '\\';
            output[position++] = '"';
        }
        else
        {
            output[position++] = c;
        }
    }

    output[position++] = '"';
    output[position] = '\0';

    return output;
}


/*
 * Devuelve el nombre del linker/driver.
 */
static const char *linker_driver_name(void)
{
    return "clang";
}


static const char *linker_driver(
    const Linker *linker
)
{
    (void)linker;

    return linker_driver_name();
}


const char *linker_compiler(void)
{
    return linker_driver_name();
}


/*
 * Comprueba si una ruta parece ser un archivo de librería
 * u objeto.
 */
static int linker_is_file(
    const char *path
)
{
    if (!path)
        return 0;

    FILE *file = fopen(path, "rb");

    if (!file)
        return 0;

    fclose(file);

    return 1;
}


/*
 * Añade un argumento escapado al comando.
 */
static int linker_append_quoted_arg(
    char **command,
    size_t *length,
    size_t *capacity,
    const char *argument
)
{
    char *quoted =
        linker_quote_argument(argument);

    if (!quoted)
        return 0;

    int result =
        linker_command_append_arg(
            command,
            length,
            capacity,
            quoted
        );

    free(quoted);

    return result;
}


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

Linker *linker_create(
    const LinkerConfig *config,
    ErrorContext *errors
)
{
    Linker *linker =
        calloc(1, sizeof(Linker));

    if (!linker)
    {
        if (errors)
        {
            error_report_simple(
                errors,
                ERROR_INTERNAL_OUT_OF_MEMORY,
                ERROR_PHASE_INTERNAL,
                0,
                0,
                "no se pudo crear el linker"
            );
        }

        return NULL;
    }

    linker->errors = errors;
    linker->had_error = 0;
    linker->command = NULL;

    if (config)
    {
        linker->config.platform =
            config->platform;

        linker->config.architecture =
            config->architecture;

        linker->config.object_file =
            linker_strdup(config->object_file);

        linker->config.output_file =
            linker_strdup(config->output_file);

        linker->config.debug_info =
            config->debug_info;

        linker->config.optimize =
            config->optimize;

        linker->config.use_runtime =
            config->use_runtime;

        linker->config.runtime_path =
            linker_strdup(config->runtime_path);

        /*
         * Librerías.
         */
        for (int i = 0;
             i < config->library_count;
             i++)
        {
            if (!linker_add_library(
                    linker,
                    config->libraries[i]))
            {
                linker_destroy(linker);
                return NULL;
            }
        }

        /*
         * Rutas de librerías.
         */
        for (int i = 0;
             i < config->library_path_count;
             i++)
        {
            if (!linker_add_library_path(
                    linker,
                    config->library_paths[i]))
            {
                linker_destroy(linker);
                return NULL;
            }
        }

        /*
         * Rutas de inclusión.
         */
        for (int i = 0;
             i < config->include_path_count;
             i++)
        {
            if (!linker_add_include_path(
                    linker,
                    config->include_paths[i]))
            {
                linker_destroy(linker);
                return NULL;
            }
        }
    }
    else
    {
        linker->config =
            linker_default_config();
    }

    return linker;
}


void linker_destroy(Linker *linker)
{
    if (!linker)
        return;

    free((void *)linker->config.object_file);
    free((void *)linker->config.output_file);

    free((void *)linker->config.runtime_path);

    linker_string_array_destroy(
        linker->config.libraries,
        linker->config.library_count
    );

    linker_string_array_destroy(
        linker->config.library_paths,
        linker->config.library_path_count
    );

    linker_string_array_destroy(
        linker->config.include_paths,
        linker->config.include_path_count
    );

    free(linker->command);

    free(linker);
}


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

LinkerConfig linker_default_config(void)
{
    LinkerConfig config;

    memset(
        &config,
        0,
        sizeof(LinkerConfig)
    );

#ifdef _WIN32
    config.platform =
        LINKER_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    config.platform =
        LINKER_PLATFORM_MACOS;
#else
    config.platform =
        LINKER_PLATFORM_LINUX;
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
    config.architecture =
        LINKER_ARCH_ARM64;
#else
    config.architecture =
        LINKER_ARCH_X86_64;
#endif

    config.object_file = NULL;
    config.output_file = NULL;

    config.libraries = NULL;
    config.library_count = 0;
    config.library_capacity = 0;

    config.library_paths = NULL;
    config.library_path_count = 0;
    config.library_path_capacity = 0;

    config.include_paths = NULL;
    config.include_path_count = 0;
    config.include_path_capacity = 0;

    config.debug_info = 0;
    config.optimize = 0;

    config.use_runtime = 0;
    config.runtime_path = NULL;

    return config;
}


int linker_set_platform(
    Linker *linker,
    LinkerPlatform platform
)
{
    if (!linker)
        return 0;

    if (platform != LINKER_PLATFORM_WINDOWS &&
        platform != LINKER_PLATFORM_LINUX &&
        platform != LINKER_PLATFORM_MACOS)
    {
        linker_report_error(
            linker,
            ERROR_CODEGEN_TARGET_ERROR,
            "plataforma de linker no válida"
        );

        return 0;
    }

    linker->config.platform = platform;

    return 1;
}


int linker_set_architecture(
    Linker *linker,
    LinkerArchitecture architecture
)
{
    if (!linker)
        return 0;

    if (architecture != LINKER_ARCH_X86_64 &&
        architecture != LINKER_ARCH_ARM64)
    {
        linker_report_error(
            linker,
            ERROR_CODEGEN_TARGET_ERROR,
            "arquitectura de linker no válida"
        );

        return 0;
    }

    linker->config.architecture =
        architecture;

    return 1;
}


int linker_set_object_file(
    Linker *linker,
    const char *filename
)
{
    if (!linker || !filename)
        return 0;

    char *copy =
        linker_strdup(filename);

    if (!copy)
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo guardar el archivo objeto"
        );

        return 0;
    }

    free((void *)linker->config.object_file);

    linker->config.object_file = copy;

    return 1;
}


int linker_set_output_file(
    Linker *linker,
    const char *filename
)
{
    if (!linker || !filename)
        return 0;

    char *copy =
        linker_strdup(filename);

    if (!copy)
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo guardar el archivo de salida"
        );

        return 0;
    }

    free((void *)linker->config.output_file);

    linker->config.output_file = copy;

    return 1;
}


int linker_add_library(
    Linker *linker,
    const char *library
)
{
    if (!linker || !library)
        return 0;

    if (!linker_string_array_add(
            &linker->config.libraries,
            &linker->config.library_count,
            &linker->config.library_capacity,
            library))
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo añadir una librería"
        );

        return 0;
    }

    return 1;
}


int linker_add_library_path(
    Linker *linker,
    const char *path
)
{
    if (!linker || !path)
        return 0;

    if (!linker_string_array_add(
            &linker->config.library_paths,
            &linker->config.library_path_count,
            &linker->config.library_path_capacity,
            path))
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo añadir una ruta de librerías"
        );

        return 0;
    }

    return 1;
}


int linker_add_include_path(
    Linker *linker,
    const char *path
)
{
    if (!linker || !path)
        return 0;

    if (!linker_string_array_add(
            &linker->config.include_paths,
            &linker->config.include_path_count,
            &linker->config.include_path_capacity,
            path))
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo añadir una ruta de inclusión"
        );

        return 0;
    }

    return 1;
}


int linker_set_runtime(
    Linker *linker,
    const char *runtime_path
)
{
    if (!linker || !runtime_path)
        return 0;

    char *copy =
        linker_strdup(runtime_path);

    if (!copy)
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo guardar la ruta del runtime"
        );

        return 0;
    }

    free((void *)linker->config.runtime_path);

    linker->config.runtime_path = copy;
    linker->config.use_runtime = 1;

    return 1;
}


/* =========================================================
 * NOMBRES
 * ========================================================= */

const char *linker_platform_name(
    LinkerPlatform platform
)
{
    switch (platform)
    {
        case LINKER_PLATFORM_WINDOWS:
            return "windows";

        case LINKER_PLATFORM_LINUX:
            return "linux";

        case LINKER_PLATFORM_MACOS:
            return "macos";

        default:
            return "unknown";
    }
}


const char *linker_architecture_name(
    LinkerArchitecture architecture
)
{
    switch (architecture)
    {
        case LINKER_ARCH_X86_64:
            return "x86_64";

        case LINKER_ARCH_ARM64:
            return "arm64";

        default:
            return "unknown";
    }
}


/* =========================================================
 * CONSTRUCCIÓN DEL COMANDO
 * ========================================================= */

int linker_build_command(
    Linker *linker
)
{
    if (!linker)
        return 0;

    free(linker->command);

    linker->command = NULL;

    /*
     * Validación.
     */

    if (!linker->config.object_file)
    {
        linker_report_error(
            linker,
            ERROR_CODEGEN_VARIABLE_ERROR,
            "no se especificó un archivo objeto"
        );

        return 0;
    }

    if (!linker->config.output_file)
    {
        linker_report_error(
            linker,
            ERROR_CODEGEN_VARIABLE_ERROR,
            "no se especificó un archivo ejecutable de salida"
        );

        return 0;
    }

    /*
     * Buffer dinámico.
     */

    size_t capacity =
        LINKER_COMMAND_INITIAL_SIZE;

    size_t length = 0;

    linker->command =
        malloc(capacity);

    if (!linker->command)
    {
        linker_report_error(
            linker,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            "no se pudo reservar memoria para el comando del linker"
        );

        return 0;
    }

    linker->command[0] = '\0';


    /*
     * Driver.
     */

    if (!linker_command_append_arg(
            &linker->command,
            &length,
            &capacity,
            linker_driver(linker)))
    {
        goto out_of_memory;
    }


    /*
     * Arquitectura.
     */

    if (linker->config.platform ==
        LINKER_PLATFORM_WINDOWS)
    {
        if (linker->config.architecture ==
            LINKER_ARCH_X86_64)
        {
            if (!linker_command_append_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    "--target=x86_64-pc-windows-msvc"))
            {
                goto out_of_memory;
            }
        }
        else
        {
            if (!linker_command_append_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    "--target=aarch64-pc-windows-msvc"))
            {
                goto out_of_memory;
            }
        }

        /*
         * Usamos LLVM/LDD como linker cuando está disponible.
         */
        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                "-fuse-ld=lld"))
        {
            goto out_of_memory;
        }
    }
    else if (linker->config.platform ==
             LINKER_PLATFORM_LINUX)
    {
        /*
         * Linux.
         *
         * Para ARM64 usamos el target de clang.
         */
        if (linker->config.architecture ==
            LINKER_ARCH_ARM64)
        {
            if (!linker_command_append_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    "--target=aarch64-linux-gnu"))
            {
                goto out_of_memory;
            }
        }
        else
        {
            if (!linker_command_append_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    "-m64"))
            {
                goto out_of_memory;
            }
        }
    }


    /*
     * Debug.
     */

    if (linker->config.debug_info)
    {
        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                "-g"))
        {
            goto out_of_memory;
        }
    }


    /*
     * Optimización.
     */

    if (linker->config.optimize > 0)
    {
        const char *optimization_flag =
            linker->config.optimize >= 3
                ? "-O3"
                : (linker->config.optimize == 2
                       ? "-O2"
                       : "-O1");

        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                optimization_flag))
        {
            goto out_of_memory;
        }
    }


    /*
     * Archivo objeto.
     */

    if (!linker_append_quoted_arg(
            &linker->command,
            &length,
            &capacity,
            linker->config.object_file))
    {
        goto out_of_memory;
    }


    /*
     * Runtime.
     *
     * runtime_path puede apuntar directamente a:
     *
     *   runtime.o
     *   libruntime.a
     *   runtime.lib
     *
     * Si se ha especificado, se añade directamente.
     */

    if (linker->config.use_runtime)
    {
        if (!linker->config.runtime_path)
        {
            linker_report_error(
                linker,
                ERROR_CODEGEN_VARIABLE_ERROR,
                "se indicó usar el runtime pero no se especificó su ruta"
            );

            return 0;
        }

        if (!linker_append_quoted_arg(
                &linker->command,
                &length,
                &capacity,
                linker->config.runtime_path))
        {
            goto out_of_memory;
        }
    }


    /*
     * Directorios de librerías.
     */

    for (int i = 0;
         i < linker->config.library_path_count;
         i++)
    {
        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                "-L"))
        {
            goto out_of_memory;
        }

        if (!linker_append_quoted_arg(
                &linker->command,
                &length,
                &capacity,
                linker->config.library_paths[i]))
        {
            goto out_of_memory;
        }
    }


    /*
     * Directorios de inclusión.
     *
     * Técnicamente son más relevantes para el compilador
     * que para el linker, pero los conservamos porque forman
     * parte de la configuración pública de LinkerConfig.
     */
    for (int i = 0;
         i < linker->config.include_path_count;
         i++)
    {
        char argument[16];

        snprintf(
            argument,
            sizeof(argument),
            "-I"
        );

        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                argument))
        {
            goto out_of_memory;
        }

        if (!linker_append_quoted_arg(
                &linker->command,
                &length,
                &capacity,
                linker->config.include_paths[i]))
        {
            goto out_of_memory;
        }
    }


    /*
     * Librerías.
     */

    for (int i = 0;
         i < linker->config.library_count;
         i++)
    {
        const char *library =
            linker->config.libraries[i];

        /*
         * Si el usuario ya pasó:
         *
         *   libfoo.a
         *   foo.a
         *   foo.lib
         *   foo.o
         *
         * lo pasamos directamente.
         *
         * En otro caso usamos -lfoo.
         */

        size_t library_length =
            strlen(library);

        int direct_file = 0;

        if (library_length >= 2)
        {
            if (library_length >= 2 &&
                strcmp(
                    library + library_length - 2,
                    ".o"
                ) == 0)
            {
                direct_file = 1;
            }

            if (library_length >= 2 &&
                strcmp(
                    library + library_length - 2,
                    ".a"
                ) == 0)
            {
                direct_file = 1;
            }

            if (library_length >= 4 &&
                strcmp(
                    library + library_length - 4,
                    ".lib"
                ) == 0)
            {
                direct_file = 1;
            }
        }

        if (direct_file || linker_is_file(library))
        {
            if (!linker_append_quoted_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    library))
            {
                goto out_of_memory;
            }
        }
        else
        {
            char *argument =
                malloc(
                    strlen(library) + 3
                );

            if (!argument)
                goto out_of_memory;

            snprintf(
                argument,
                strlen(library) + 3,
                "-l%s",
                library
            );

            if (!linker_command_append_arg(
                    &linker->command,
                    &length,
                    &capacity,
                    argument))
            {
                free(argument);
                goto out_of_memory;
            }

            free(argument);
        }
    }


    /*
     * Libreria matematica de C.
     *
     * Linux requiere enlazar libm explícitamente; macOS la
     * incluye en libSystem y Windows en su runtime de C.
     */
    if (linker->config.platform ==
        LINKER_PLATFORM_LINUX)
    {
        if (!linker_command_append_arg(
                &linker->command,
                &length,
                &capacity,
                "-lm"))
        {
            goto out_of_memory;
        }
    }


    /*
     * Archivo de salida.
     */

    if (!linker_command_append_arg(
            &linker->command,
            &length,
            &capacity,
            "-o"))
    {
        goto out_of_memory;
    }

    if (!linker_append_quoted_arg(
            &linker->command,
            &length,
            &capacity,
            linker->config.output_file))
    {
        goto out_of_memory;
    }


    return 1;


out_of_memory:

    linker_report_error(
        linker,
        ERROR_INTERNAL_OUT_OF_MEMORY,
        "no se pudo construir el comando del linker"
    );

    free(linker->command);

    linker->command = NULL;

    return 0;
}


/* =========================================================
 * COMPROBAR LINKER
 * ========================================================= */

/*
 * Busca el driver en PATH sin lanzar un proceso: ejecutar
 * "clang --version" en cada compilación costaba ~15 ms.
 */
static int linker_driver_exists(const char *driver)
{
    if (!driver || driver[0] == '\0')
        return 0;

    if (strchr(driver, '/') || strchr(driver, '\\'))
        return RIVO_ACCESS_X(driver) == 0;

    const char *path = getenv("PATH");

    if (!path)
        return 0;

#ifdef _WIN32
    const char separator = ';';
    const char *suffix = ".exe";
#else
    const char separator = ':';
    const char *suffix = "";
#endif

    size_t driver_length = strlen(driver);
    size_t suffix_length = strlen(suffix);

    while (*path) {

        const char *end = strchr(path, separator);
        size_t directory_length = end
            ? (size_t)(end - path)
            : strlen(path);

        if (directory_length > 0) {
            size_t total = directory_length + 1 +
                driver_length + suffix_length + 1;

            char *candidate = malloc(total);

            if (!candidate)
                return 0;

            memcpy(candidate, path, directory_length);
            candidate[directory_length] = '/';
            memcpy(candidate + directory_length + 1,
                driver, driver_length);
            memcpy(candidate + directory_length + 1 + driver_length,
                suffix, suffix_length + 1);

            int exists = RIVO_ACCESS_X(candidate) == 0;

            free(candidate);

            if (exists)
                return 1;
        }

        if (!end)
            break;

        path = end + 1;
    }

    return 0;
}


int linker_check_available(
    Linker *linker
)
{
    if (!linker)
        return 0;

    const char *driver =
        linker_driver(linker);

    if (!linker_driver_exists(driver))
    {
        linker_report_errorf(
            linker,
            ERROR_CODEGEN_TARGET_ERROR,
            "no se encontró el linker '%s'",
            driver
        );

        return 0;
    }

    return 1;
}


/* =========================================================
 * EJECUTAR LINKER
 * ========================================================= */

int linker_execute(
    Linker *linker
)
{
    if (!linker)
        return 0;

    if (!linker->command)
    {
        if (!linker_build_command(linker))
            return 0;
    }

    int result =
        system(linker->command);

    if (result != 0)
    {
        linker_report_errorf(
            linker,
            ERROR_CODEGEN_TARGET_ERROR,
            "el linker terminó con código de error: %d",
            result
        );

        return 0;
    }

    return 1;
}


/* =========================================================
 * LINK COMPLETO
 * ========================================================= */

int linker_link(
    Linker *linker
)
{
    if (!linker)
        return 0;

    linker->had_error = 0;

    /*
     * Primero comprobamos que clang exista.
     */
    if (!linker_check_available(linker))
        return 0;

    /*
     * Construimos el comando.
     */
    if (!linker_build_command(linker))
        return 0;

    /*
     * Ejecutamos.
     */
    if (!linker_execute(linker))
        return 0;

    return 1;
}


/* =========================================================
 * INFORMACIÓN
 * ========================================================= */

const char *linker_get_command(
    const Linker *linker
)
{
    if (!linker)
        return NULL;

    return linker->command;
}


int linker_has_error(
    const Linker *linker
)
{
    if (!linker)
        return 1;

    return linker->had_error;
}