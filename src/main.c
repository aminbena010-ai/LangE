#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "llvm.h"
#include "linker.h"
#include "error.h"
#include "manifest.h"
#include "lockfile.h"
#include "packages.h"
#include "build.h"
#include "formatter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <sys/wait.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif


/* =========================================================
 * CONSTANTES
 * ========================================================= */

#include "version.h"

#ifdef _WIN32
#define RIVO_DEFAULT_OUTPUT "a.exe"
#else
#define RIVO_DEFAULT_OUTPUT "a.out"
#endif
#define RIVO_DEFAULT_OBJECT "programa.o"

#if defined(_WIN32)
#undef PATH_MAX
#define PATH_MAX 32768
#elif !defined(PATH_MAX)
#define PATH_MAX 32768
#endif


/* =========================================================
 * RUTAS INTERNAS
 *
 * El compilador lleva la ruta de la biblioteca estándar
 * (RIVO_STDLIB_PATH) y la del runtime (RIVO_RUNTIME_PATH)
 * fijadas al compilar. Si el binario se mueve (por ejemplo
 * con `make install` a ~/.local), esas rutas ya no valen:
 * entonces se derivan del propio ejecutable:
 *
 *   <dir-del-binario>/../share/rivo/lib   stdlib .ro
 *   <dir-del-binario>/../lib/librivo_runtime.a/.lib   runtime
 *
 * Sin variables de entorno ni configuración externa.
 * ========================================================= */

static int rivo_path_exists(const char *path)
{
#ifdef _WIN32
    return path != NULL && _access(path, 0) == 0;
#else
    return path != NULL && access(path, F_OK) == 0;
#endif
}

/* Directorio que contiene al ejecutable. */
static int rivo_exe_dir(char *buf, size_t size)
{
    char exe[PATH_MAX];
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(NULL, exe, (DWORD) sizeof(exe));

    if (length == 0 || length >= sizeof(exe))
        return 0;

    exe[length] = '\0';
    char *slash = strrchr(exe, '\\');
    char *forward_slash = strrchr(exe, '/');
    if (forward_slash && (!slash || forward_slash > slash))
        slash = forward_slash;
#elif defined(__APPLE__)
    uint32_t length = (uint32_t) sizeof(exe);

    if (_NSGetExecutablePath(exe, &length) != 0)
        return 0;

    char *slash = strrchr(exe, '/');
#else
    ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

    if (length <= 0)
        return 0;

    exe[length] = '\0';
    char *slash = strrchr(exe, '/');
#endif

    if (slash == NULL)
        return 0;

    *slash = '\0';

    if (strlen(exe) + 1 > size)
        return 0;

    memcpy(buf, exe, strlen(exe) + 1);

    return 1;
}

/* Ruta de la stdlib: instalada (relativa al binario) si
 * existe, si no la fijada al compilar. */
static const char *rivo_stdlib_path(void)
{
    static char installed[PATH_MAX];
    static int resolved = 0;
    static const char *chosen = NULL;

    if (resolved)
        return chosen;

    resolved = 1;

#ifdef RIVO_STDLIB_PATH
    chosen = RIVO_STDLIB_PATH;
#endif

    char dir[PATH_MAX];

    if (rivo_exe_dir(dir, sizeof(dir) - 64)) {
        snprintf(installed, sizeof(installed),
                 "%s/../share/rivo/lib", dir);

        if (rivo_path_exists(installed))
            chosen = installed;
    }

    return chosen;
}

/* Ruta del runtime: instalado si existe,
 * si no el fijado al compilar. */
static const char *rivo_runtime_path(void)
{
    static char installed[PATH_MAX];
    static int resolved = 0;
    static const char *chosen = NULL;

    if (resolved)
        return chosen;

    resolved = 1;

#ifdef RIVO_RUNTIME_PATH
    chosen = RIVO_RUNTIME_PATH;
#endif

    char dir[PATH_MAX];

    if (rivo_exe_dir(dir, sizeof(dir) - 64)) {
#ifdef _WIN32
        snprintf(installed, sizeof(installed),
                 "%s/../lib/librivo_runtime.lib", dir);
#else
        snprintf(installed, sizeof(installed),
                 "%s/../lib/librivo_runtime.a", dir);
#endif

        if (rivo_path_exists(installed))
            chosen = installed;
    }

    return chosen;
}


/* =========================================================
 * ESTRUCTURA DE OPCIONES
 * ========================================================= */

typedef struct CompilerOptions {

    const char *input_file;
    const char *output_file;
    const char *object_file;

    int emit_ir;
    int emit_assembly;
    int emit_object;

    int keep_object;

    int debug_info;
    int optimize;

    /*
     * 1 si -g / -O* se pasaron en la línea de comandos.
     * Sirve para que el manifiesto rivo.toml no pise
     * una opción dada explícitamente por el usuario.
     */
    int debug_set;
    int optimize_set;

    int verbose;

    /*
     * Directorios adicionales (-I): se usan para buscar
     * módulos .ro importados y cabeceras de los
     * ficheros C declarados con `external`.
     */
    const char **include_dirs;
    int include_count;

    /*
     * Bibliotecas (-l) y rutas de bibliotecas (-L) para el
     * enlace final. Se pasan a linker_add_library() y
     * linker_add_library_path().
     */
    const char **libraries;
    int library_count;

    const char **library_paths;
    int library_path_count;

    /*
     * --run: ejecutar el binario tras compilar, al estilo
     * de `cargo run`. Los argumentos tras `--` se pasan
     * al programa.
     */
    int run_after;
    const char **run_args;
    int run_arg_count;

    /*
     * --ffi-check: activar la verificación ABI de las
     * funciones `external` (generación de header con las
     * firmas esperadas + -include al compilar el .c).
     * Desactivado por defecto: C no distingue entre
     * long long / int64_t / long como tipos compatibles.
     */
    int ffi_check;

} CompilerOptions;


/* =========================================================
 * UTILIDADES
 * ========================================================= */

static char *string_duplicate(const char *value)
{
    if (!value)
        return NULL;

    size_t length = strlen(value);

    char *copy =
        malloc(length + 1);

    if (!copy)
        return NULL;

    memcpy(
        copy,
        value,
        length + 1
    );

    return copy;
}


static char *read_file(
    const char *filename,
    size_t *out_size
)
{
    if (!filename)
        return NULL;

    FILE *file =
        fopen(filename, "rb");

    if (!file)
    {
        fprintf(
            stderr,
            "rivo: no se pudo abrir '%s'\n",
            filename
        );

        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    long size =
        ftell(file);

    if (size < 0)
    {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer =
        malloc((size_t)size + 1);

    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    size_t read =
        fread(
            buffer,
            1,
            (size_t)size,
            file
        );

    fclose(file);

    buffer[read] = '\0';

    if (out_size)
        *out_size = read;

    return buffer;
}


/*
 * Devuelve una ruta de salida basada en el nombre
 * del archivo de entrada.
 *
 * programa.ro
 *     ↓
 * programa
 */
static char *default_output_name(
    const char *input
)
{
    if (!input)
        return string_duplicate(
            RIVO_DEFAULT_OUTPUT
        );

    const char *filename =
        input;

    const char *slash =
        strrchr(input, '/');

#ifdef _WIN32
    const char *backslash =
        strrchr(input, '\\');

    if (backslash &&
        (!slash || backslash > slash))
    {
        filename = backslash + 1;
    }
    else if (slash)
    {
        filename = slash + 1;
    }
#else
    if (slash)
        filename = slash + 1;
#endif

    size_t length =
        strlen(filename);

    if (length > 3 &&
        strcmp(
            filename + length - 3,
            ".ro"
        ) == 0)
    {
        length -= 3;
    }

    char *output =
        malloc(length + 1);

    if (!output)
        return NULL;

    memcpy(
        output,
        filename,
        length
    );

    output[length] = '\0';

#ifdef _WIN32
    /*
     * En Windows generamos .exe.
     */
    if (length + 4 + 1 > length + 1)
    {
        char *with_extension =
            malloc(length + 5);

        if (!with_extension)
        {
            free(output);
            return NULL;
        }

        memcpy(
            with_extension,
            output,
            length
        );

        memcpy(
            with_extension + length,
            ".exe",
            5
        );

        free(output);

        return with_extension;
    }
#endif

    return output;
}


/* =========================================================
 * USO
 * ========================================================= */

static void print_usage(
    const char *program
)
{
    printf(
        "E++ %s\n\n"
        "Uso:\n"
        "  %s <archivo.ro> [opciones]\n"
        "  %s fmt [--check] [archivo.ro | directorio ...]\n\n"

        "Opciones:\n"
        "  -o <archivo>       Nombre del ejecutable\n"
        "  -I <dir>           Directorio extra de módulos\n"
        "  -l <lib>           Biblioteca para el enlace\n"
        "  -L <dir>           Directorio extra de bibliotecas\n"
        "  -c                 Generar objeto sin enlazar\n"
        "  -S                 Generar ensamblador\n"
        "  -emit-ir           Generar LLVM IR\n"
        "  -g                 Incluir información de depuración\n"
        "  -O0                Sin optimización\n"
        "  -O1                Optimización básica\n"
        "  -O2, -O            Optimización\n"
        "  -O3                Optimización agresiva\n"
        "  -v, --verbose      Mostrar información del compilador\n"
        "  --keep-object      No borrar el archivo .o\n"
        "  --ffi-check        Activar verificación ABI de externals\n"
        "  --run              Ejecutar el binario tras compilar\n"
        "  --                 Argumentos para el programa (con --run)\n"
        "  --version          Mostrar versión\n"
        "  -h, --help         Mostrar esta ayuda\n\n"

        "Paquetes:\n"
        "  %s install         Resolver dependencias y escribir rivo.lock\n"
        "  %s update          Recalcular rivo.lock\n"
        "  %s add <dep>       Añadir dependencia (nombre@version o ruta)\n"
        "  %s remove <dep>    Eliminar dependencia\n"
        "  %s tree            Mostrar el grafo de dependencias\n"
        "  %s list            Listar dependencias del lock\n"
        "  %s search <texto>  Buscar en el registro\n"
        "  %s info <paquete>  Ver versiones publicadas\n"
        "  %s publish <tag>   Publicar en el registro Git\n\n"

        "Si existe un archivo rivo.toml junto al programa\n"
        "(o en un directorio superior), se usa como\n"
        "manifiesto de proyecto: nombre, versión, entrada,\n"
        "dependencias y opciones por defecto.\n\n",

        RIVO_VERSION_FULL,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program
    );
}


static void print_version(void)
{
    printf(
        "E++ %s\n",
        RIVO_VERSION_FULL
    );
}


/* =========================================================
 * PARSEAR ARGUMENTOS
 * ========================================================= */

static int parse_arguments(
    int argc,
    char **argv,
    CompilerOptions *options
)
{
    if (!options)
        return 0;

    memset(
        options,
        0,
        sizeof(CompilerOptions)
    );

    options->output_file = NULL;
    options->object_file = NULL;

    options->emit_ir = 0;
    options->emit_assembly = 0;
    options->emit_object = 0;

    options->keep_object = 0;

    options->debug_info = 0;
    options->optimize = 0;

    options->verbose = 0;

    options->include_dirs = NULL;
    options->include_count = 0;

    options->libraries = NULL;
    options->library_count = 0;

    options->library_paths = NULL;
    options->library_path_count = 0;

    options->run_after = 0;
    options->run_args = NULL;
    options->run_arg_count = 0;

    int raw_arguments = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *argument =
            argv[i];

        /*
         * Tras `--`, todo es para el programa ejecutado
         * con --run.
         */
        if (raw_arguments)
        {
            const char **grown =
                realloc(
                    options->run_args,
                    sizeof(const char *) *
                    (size_t)(options->run_arg_count + 1)
                );

            if (grown)
            {
                grown[options->run_arg_count++] =
                    argument;
                options->run_args = grown;
            }

            continue;
        }

        if (strcmp(argument, "--") == 0)
        {
            raw_arguments = 1;
            continue;
        }

        /*
         * Ejecutar tras compilar (como cargo run).
         */
        if (strcmp(argument, "--run") == 0)
        {
            options->run_after = 1;
            continue;
        }

        /*
         * Ayuda.
         */
        if (strcmp(argument, "-h") == 0 ||
            strcmp(argument, "--help") == 0)
        {
            print_usage(argv[0]);
            exit(0);
        }

        /*
         * Versión.
         */
        if (strcmp(argument, "--version") == 0)
        {
            print_version();
            exit(0);
        }

        /*
         * Archivo de salida.
         */
        if (strcmp(argument, "-o") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(
                    stderr,
                    "rivo: falta el archivo después de '-o'\n"
                );

                return 0;
            }

            options->output_file =
                argv[++i];

            continue;
        }

        /*
         * Directorio de búsqueda de módulos y cabeceras:
         * -I dir o -Idir.
         */
        int include_argument = 0;

        if (strcmp(argument, "-I") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(
                    stderr,
                    "rivo: falta el directorio después de '-I'\n"
                );

                return 0;
            }

            argument = argv[++i];
            include_argument = 1;

        } else if (strncmp(argument, "-I", 2) == 0 &&
                   argument[2] != '\0')
        {
            argument += 2;
            include_argument = 1;
        }

        if (include_argument)
        {
            const char **grown =
                realloc(
                    options->include_dirs,
                    sizeof(const char *) *
                    (size_t)(options->include_count + 1)
                );

            if (grown == NULL)
            {
                fprintf(
                    stderr,
                    "rivo: sin memoria para el directorio '%s'\n",
                    argument
                );

                return 0;
            }

            options->include_dirs = grown;
            options->include_dirs[
                options->include_count++]
                = argument;

            continue;
        }

        /*
         * Biblioteca para el enlace: -l nombre o -lnombre.
         */
        int library_argument = 0;

        if (strcmp(argument, "-l") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(
                    stderr,
                    "rivo: falta la biblioteca despues de '-l'\n"
                );

                return 0;
            }

            argument = argv[++i];
            library_argument = 1;

        } else if (strncmp(argument, "-l", 2) == 0 &&
                   argument[2] != '\0')
        {
            argument += 2;
            library_argument = 1;
        }

        if (library_argument)
        {
            const char **grown =
                realloc(
                    options->libraries,
                    sizeof(const char *) *
                    (size_t)(options->library_count + 1)
                );

            if (grown == NULL)
            {
                fprintf(
                    stderr,
                    "rivo: sin memoria para la biblioteca '%s'\n",
                    argument
                );

                return 0;
            }

            options->libraries = grown;
            options->libraries[
                options->library_count++]
                = argument;

            continue;
        }

        /*
         * Ruta de bibliotecas para el enlace: -L dir o -Ldir.
         */
        int library_path_argument = 0;

        if (strcmp(argument, "-L") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(
                    stderr,
                    "rivo: falta el directorio despues de '-L'\n"
                );

                return 0;
            }

            argument = argv[++i];
            library_path_argument = 1;

        } else if (strncmp(argument, "-L", 2) == 0 &&
                   argument[2] != '\0')
        {
            argument += 2;
            library_path_argument = 1;
        }

        if (library_path_argument)
        {
            const char **grown =
                realloc(
                    options->library_paths,
                    sizeof(const char *) *
                    (size_t)(options->library_path_count + 1)
                );

            if (grown == NULL)
            {
                fprintf(
                    stderr,
                    "rivo: sin memoria para el directorio '%s'\n",
                    argument
                );

                return 0;
            }

            options->library_paths = grown;
            options->library_paths[
                options->library_path_count++]
                = argument;

            continue;
        }

        /*
         * Generar objeto.
         */
        if (strcmp(argument, "-c") == 0)
        {
            options->emit_object = 1;
            continue;
        }

        /*
         * Generar assembly.
         */
        if (strcmp(argument, "-S") == 0)
        {
            options->emit_assembly = 1;
            continue;
        }

        /*
         * Generar LLVM IR.
         */
        if (strcmp(argument, "-emit-ir") == 0)
        {
            options->emit_ir = 1;
            continue;
        }

        /*
         * Debug.
         */
        if (strcmp(argument, "-g") == 0)
        {
            options->debug_info = 1;
            options->debug_set = 1;
            continue;
        }

        /*
         * Optimización.
         */
        if (strcmp(argument, "-O0") == 0)
        {
            options->optimize = 0;
            options->optimize_set = 1;
            continue;
        }

        if (strcmp(argument, "-O1") == 0)
        {
            options->optimize = 1;
            options->optimize_set = 1;
            continue;
        }

        if (strcmp(argument, "-O2") == 0 ||
            strcmp(argument, "-O") == 0)
        {
            options->optimize = 2;
            options->optimize_set = 1;
            continue;
        }

        if (strcmp(argument, "-O3") == 0)
        {
            options->optimize = 3;
            options->optimize_set = 1;
            continue;
        }

        /*
         * Verbose.
         */
        if (strcmp(argument, "-v") == 0 ||
            strcmp(argument, "--verbose") == 0)
        {
            options->verbose = 1;
            continue;
        }

        /*
         * Mantener .o.
         */
        if (strcmp(argument, "--keep-object") == 0)
        {
            options->keep_object = 1;
            continue;
        }

        /*
         * Activar verificación ABI FFI.
         */
        if (strcmp(argument, "--ffi-check") == 0)
        {
            options->ffi_check = 1;
            continue;
        }

        /*
         * Argumento desconocido.
         */
        if (argument[0] == '-')
        {
            fprintf(
                stderr,
                "rivo: opción desconocida '%s'\n",
                argument
            );

            return 0;
        }

        /*
         * Archivo de entrada.
         */
        if (!options->input_file)
        {
            options->input_file =
                argument;
        }
        else
        {
            fprintf(
                stderr,
                "rivo: demasiados archivos de entrada\n"
            );

            return 0;
        }
    }

    /*
     * La ausencia de archivo de entrada se comprueba en
     * main(), después de intentar completarlo con el
     * manifiesto rivo.toml.
     */

    /*
     * La extensión oficial de los archivos fuente de E++
     * es .ro.
     */
    if (options->input_file) {

        size_t input_length =
            strlen(options->input_file);

        if (input_length < 3 ||
            strcmp(
                options->input_file + input_length - 3,
                ".ro"
            ) != 0)
        {
            fprintf(
                stderr,
                "rivo: la extensión oficial de los archivos "
                "fuente es '.ro'\n"
            );

            return 0;
        }
    }

    return 1;
}


/* =========================================================
 * INFORMACIÓN VERBOSE
 * ========================================================= */

static void verbose_stage(
    const CompilerOptions *options,
    const char *message
)
{
    if (!options || !options->verbose)
        return;

    printf(
        "[rivo] %s\n",
        message
    );
}


/* =========================================================
 * MÓDULOS (import / from ... import)
 *
 * Un módulo es un archivo .ro situado junto al archivo
 * principal. Se carga en la misma unidad de compilación.
 * ========================================================= */

typedef struct ModuleLoader {

    /* Errores del compilador */
    ErrorContext *errors;

    /* Directorio del archivo principal */
    char *base_dir;

    /*
     * Directorios donde buscar `modulo.ro`, en orden.
     * El primero es siempre base_dir.
     */
    char **search_dirs;
    int search_dir_count;

    /* Módulos ya cargados */
    char **loaded;
    int loaded_count;

    /*
     * Rutas de los ficheros .ro realmente cargados
     * (principal + imports): se usan para la caché de
     * target/. La propiedad pasa al llamador.
     */
    char **sources;
    int source_count;

    /* Módulos en curso (detecta importaciones cíclicas) */
    char **stack;
    int stack_count;

} ModuleLoader;


static int module_list_contains(
    char **list,
    int count,
    const char *module
)
{
    for (int i = 0; i < count; i++) {

        if (list[i] != NULL &&
            strcmp(list[i], module) == 0)
            return 1;
    }

    return 0;
}


static int module_list_add(
    char ***list,
    int *count,
    const char *module
)
{
    char **grown =
        realloc(
            *list,
            (size_t)(*count + 1) * sizeof(char *)
        );

    if (!grown)
        return 0;

    char *copy = string_duplicate(module);

    if (!copy) {
        *list = grown;
        return 0;
    }

    grown[*count] = copy;
    *list = grown;
    (*count)++;

    return 1;
}


/* Definidas más abajo, con los ficheros C externos. */
static int source_file_exists(const char *path);
static int source_path_is_absolute(const char *path);
static char *source_join_path(
    const char *directory,
    const char *path
);

/* Definida más abajo, con los módulos. */
static char *program_directory(const char *input_file);


static char *module_build_path(
    const char *directory,
    const char *module
)
{
    size_t directory_length =
        strlen(directory);

    size_t module_length =
        strlen(module);

    char *path =
        malloc(
            directory_length + 1 +
            module_length + 5
        );

    if (!path)
        return NULL;

    memcpy(
        path,
        directory,
        directory_length
    );

    path[directory_length] = '/';

    memcpy(
        path + directory_length + 1,
        module,
        module_length
    );

    memcpy(
        path + directory_length + 1 + module_length,
        ".ro",
        4
    );

    path[directory_length + 1 + module_length + 3] = '\0';

    return path;
}


/*
 * Busca `modulo.ro` en los directorios de búsqueda, en
 * este orden: el del programa, los de -I, RIVO_LIB y la
 * biblioteca estándar. Si no está en ninguno, se devuelve
 * la ruta del primer directorio para que el mensaje de
 * "módulo no encontrado" la muestre.
 */
static char *module_file_path(
    const ModuleLoader *loader,
    const char *module
)
{
    char *fallback = NULL;

    for (int i = 0;
         i < loader->search_dir_count;
         i++)
    {
        char *path =
            module_build_path(
                loader->search_dirs[i],
                module
            );

        if (path == NULL)
            return NULL;

        if (source_file_exists(path))
            return path;

        if (fallback == NULL)
            fallback = path;
        else
            free(path);
    }

    if (fallback != NULL)
        return fallback;

    return module_build_path(
        loader->base_dir,
        module
    );
}


/*
 * Las rutas `external "..."` de un módulo se resuelven
 * relativas al directorio donde está el módulo, no al del
 * programa: así la biblioteca estándar puede llevar sus
 * propios ficheros C.
 */
static void external_resolve_in_module(
    ASTNodeList *declarations,
    const char *module_dir
)
{
    if (declarations == NULL ||
        module_dir == NULL)
        return;

    for (int i = 0;
         i < declarations->count;
         i++)
    {
        ASTNode *child =
            declarations->items[i];

        if (child == NULL ||
            child->kind != AST_FUNCTION_DECL)
            continue;

        char *declared =
            child->data.function_decl.external_path;

        if (declared == NULL ||
            source_path_is_absolute(declared))
            continue;

        char *resolved =
            source_join_path(module_dir, declared);

        if (resolved == NULL)
            continue;

        if (source_file_exists(resolved)) {

            child->data.function_decl
                .external_path = resolved;

            free(declared);

        } else {

            free(resolved);
        }
    }
}


static ASTNodeList *import_node_declarations(
    ASTNode *import_node
)
{
    if (import_node == NULL)
        return NULL;

    if (import_node->kind == AST_IMPORT)
        return &import_node->data.import_decl.declarations;

    if (import_node->kind == AST_FROM_IMPORT)
        return &import_node->data.from_import.declarations;

    return NULL;
}


static const char *import_node_module(
    ASTNode *import_node
)
{
    if (import_node == NULL)
        return NULL;

    if (import_node->kind == AST_IMPORT)
        return import_node->data.import_decl.module;

    if (import_node->kind == AST_FROM_IMPORT)
        return import_node->data.from_import.module;

    return NULL;
}


/*
 * Nombres historicos en español de la biblioteca estandar.
 *
 * La stdlib se renombró al inglés. Si un import usa un nombre
 * viejo, el error sugiere el canónico en vez de fallar en seco.
 */
typedef struct StdlibAlias {

    const char *old_name;
    const char *new_name;

} StdlibAlias;


static const StdlibAlias stdlib_aliases[] = {
    { "matematicas", "math" },
    { "texto",       "text" },
    { "tiempo",      "time" },
    { "aleatorio",   "random" },
    { "ficheros",    "files" },
    { "fecha",       "date" },
    { "sistema",     "system" },
    { "estadistica", "statistics" },
    { "validacion",  "validation" },
    { "rutas",       "paths" }
};


#define STDLIB_ALIAS_COUNT \
    (sizeof(stdlib_aliases) / sizeof(stdlib_aliases[0]))


/*
 * Devuelve el nombre canonico si module es un alias histórico
 * en español; NULL en caso contrario.
 */
static const char *stdlib_canonical_name(
    const char *module
)
{
    if (module == NULL)
        return NULL;

    for (size_t i = 0; i < STDLIB_ALIAS_COUNT; i++) {

        if (strcmp(module, stdlib_aliases[i].old_name) == 0)
            return stdlib_aliases[i].new_name;
    }

    return NULL;
}


static int load_module(
    ModuleLoader *loader,
    const char *module,
    ASTNode *import_node
)
{
    if (loader == NULL ||
        import_node == NULL)
        return 0;

    int line = import_node->line;
    int column = import_node->column;

    if (module == NULL) {

        error_report_simple(
            loader->errors,
            ERROR_SEM_MODULE_NOT_FOUND,
            ERROR_PHASE_SEMANTIC,
            line,
            column,
            "modulo invalido"
        );

        return 0;
    }


    /* Importación cíclica */

    if (module_list_contains(
            loader->stack,
            loader->stack_count,
            module))
    {
        char message[512];

        snprintf(
            message,
            sizeof(message),
            "import circular detectado: '%s'",
            module
        );

        error_report_simple(
            loader->errors,
            ERROR_SEM_IMPORT_NOT_FOUND,
            ERROR_PHASE_SEMANTIC,
            line,
            column,
            message
        );

        return 0;
    }


    /*
     * Si el módulo ya se cargó, las declaraciones de este
     * nodo quedan vacías: la semántica resuelve contra el
     * primer registro.
     */
    if (module_list_contains(
            loader->loaded,
            loader->loaded_count,
            module))
        return 1;


    char *path =
        module_file_path(loader, module);

    if (!path) {

        error_report_simple(
            loader->errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            line,
            column,
            "no se pudo construir la ruta del modulo"
        );

        return 0;
    }

    size_t module_size = 0;

    char *module_dir =
        program_directory(path);

    char *module_source =
        read_file(path, &module_size);

    if (!module_source) {

        char message[512];

        snprintf(
            message,
            sizeof(message),
            "module not found '%s' (esperado en '%s')",
            module,
            path
        );

        const char *canonical =
            stdlib_canonical_name(module);

        if (canonical != NULL) {

            char suggestion[256];

            snprintf(
                suggestion,
                sizeof(suggestion),
                "quizas quisiste decir '%s' (la biblioteca se renombro; antes era '%s')",
                canonical,
                module
            );

            error_report_full(
                loader->errors,
                ERROR_SEM_MODULE_NOT_FOUND,
                ERROR_PHASE_SEMANTIC,
                line,
                column,
                1,
                message,
                NULL,
                suggestion
            );

        } else {

            error_report_simple(
                loader->errors,
                ERROR_SEM_MODULE_NOT_FOUND,
                ERROR_PHASE_SEMANTIC,
                line,
                column,
                message
            );
        }

        free(module_dir);
        free(path);
        return 0;
    }


    /* Registrar la ruta para la caché de target/ */

    if (!module_list_add(
            &loader->sources,
            &loader->source_count,
            path))
    {
        error_report_simple(
            loader->errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            line,
            column,
            "no se pudo registrar la fuente del modulo"
        );

        free(module_source);
        free(module_dir);
        free(path);
        return 0;
    }


    if (!module_list_add(
            &loader->stack,
            &loader->stack_count,
            module))
    {
        error_report_simple(
            loader->errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            line,
            column,
            "no se pudo reservar memoria para el modulo"
        );

        free(module_source);
        free(module_dir);
        free(path);
        return 0;
    }


    Lexer *module_lexer =
        lexer_create(module_source);

    lexer_set_filename(
        module_lexer,
        path
    );

    Parser *module_parser =
        module_lexer != NULL
            ? parser_create(module_lexer)
            : NULL;

    ASTNode *module_program =
        module_parser != NULL
            ? parser_parse(module_parser)
            : NULL;

    int ok = 1;

    if (!module_program ||
        parser_has_error(module_parser))
    {
        char message[512];

        snprintf(
            message,
            sizeof(message),
            "error de sintaxis en el modulo '%s'",
            module
        );

        error_report_simple(
            loader->errors,
            ERROR_PARSE_INVALID_IMPORT,
            ERROR_PHASE_PARSER,
            line,
            column,
            message
        );

        ok = 0;
    }

    if (ok) {

        ASTNodeList *declarations =
            import_node_declarations(
                import_node
            );

        /*
         * Transferir la propiedad de las declaraciones
         * del módulo al nodo import.
         */
        *declarations =
            module_program
                ->data.program.declarations;

        ast_node_list_init(
            &module_program
                ->data.program.declarations
        );


        /*
         * Las rutas `external` del módulo se resuelven
         * contra el directorio donde está el módulo.
         */
        external_resolve_in_module(
            declarations,
            module_dir
        );


        /* Imports anidados (DFS) */

        for (int i = 0;
             i < declarations->count && ok;
             i++)
        {
            ASTNode *child =
                declarations->items[i];

            if (child == NULL)
                continue;

            if (child->kind == AST_IMPORT ||
                child->kind == AST_FROM_IMPORT)
            {
                ok = load_module(
                    loader,
                    import_node_module(child),
                    child
                );
            }
        }

        if (ok) {

            ok = module_list_add(
                &loader->loaded,
                &loader->loaded_count,
                module
            );

            if (!ok) {

                error_report_simple(
                    loader->errors,
                    ERROR_INTERNAL_OUT_OF_MEMORY,
                    ERROR_PHASE_INTERNAL,
                    line,
                    column,
                    "no se pudo registrar el modulo"
                );
            }
        }
    }


    if (module_program)
        ast_destroy(module_program);

    if (module_parser)
        parser_destroy(module_parser);

    if (module_lexer)
        lexer_destroy(module_lexer);

    free(module_source);
    free(module_dir);
    free(path);

    loader->stack_count--;

    return ok;
}


/*
 * Directorio del archivo principal (o "." si no tiene).
 *
 * Se usa para localizar los módulos importados y los
 * ficheros C declarados con `external`.
 */
static char *program_directory(
    const char *input_file
)
{
    if (input_file == NULL)
        return string_duplicate(".");

    const char *slash =
        strrchr(input_file, '/');

#ifdef _WIN32
    const char *backslash =
        strrchr(input_file, '\\');

    if (backslash &&
        (!slash || backslash > slash))
        slash = backslash;
#endif

    if (slash == NULL)
        return string_duplicate(".");

    size_t length =
        (size_t)(slash - input_file);

    char *directory =
        malloc(length + 1);

    if (directory == NULL)
        return NULL;

    memcpy(directory, input_file, length);
    directory[length] = '\0';

    return directory;
}


static int load_program_modules(
    ASTNode *program,
    const char *input_file,
    const CompilerOptions *options,
    ErrorContext *errors,
    char ***out_sources,
    int *out_source_count
)
{
    if (out_sources)
        *out_sources = NULL;

    if (out_source_count)
        *out_source_count = 0;

    if (program == NULL)
        return 0;

    ModuleLoader loader;
    memset(&loader, 0, sizeof(loader));

    loader.errors = errors;


    /* Directorio del archivo principal */

    loader.base_dir =
        program_directory(input_file);

    if (!loader.base_dir) {

        error_report_simple(
            errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            0,
            0,
            "no se pudo resolver el directorio de modulos"
        );

        return 0;
    }


    /*
     * Directorios de búsqueda de módulos, en orden:
     * el del programa, los de -I, la variable RIVO_LIB y
     * la biblioteca estándar.
     */

    int dirs_ok =
        module_list_add(
            &loader.search_dirs,
            &loader.search_dir_count,
            loader.base_dir
        );

    if (dirs_ok && options != NULL) {

        for (int i = 0;
             i < options->include_count && dirs_ok;
             i++)
        {
            dirs_ok = module_list_add(
                &loader.search_dirs,
                &loader.search_dir_count,
                options->include_dirs[i]
            );
        }
    }

    const char *library_env =
        getenv("RIVO_LIB");

    if (dirs_ok &&
        library_env != NULL &&
        library_env[0] != '\0')
    {
        dirs_ok = module_list_add(
            &loader.search_dirs,
            &loader.search_dir_count,
            library_env
        );
    }

#ifdef RIVO_STDLIB_PATH
    if (dirs_ok) {

        dirs_ok = module_list_add(
            &loader.search_dirs,
            &loader.search_dir_count,
            rivo_stdlib_path()
        );
    }
#endif

    if (!dirs_ok) {

        error_report_simple(
            errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            0,
            0,
            "no se pudieron preparar los directorios de modulos"
        );

        free(loader.base_dir);
        return 0;
    }


    int ok = 1;

    for (int i = 0;
         i < program->data.program.declarations.count && ok;
         i++)
    {
        ASTNode *node =
            program
                ->data.program.declarations.items[i];

        if (node == NULL)
            continue;

        if (node->kind == AST_IMPORT ||
            node->kind == AST_FROM_IMPORT)
        {
            ok = load_module(
                &loader,
                import_node_module(node),
                node
            );
        }
    }


    for (int i = 0; i < loader.loaded_count; i++)
        free(loader.loaded[i]);

    free(loader.loaded);

    /*
     * Las rutas de las fuentes cargadas pasan al llamador
     * para la caché de target/.
     */
    if (ok && out_sources && loader.sources) {

        *out_sources = loader.sources;

        if (out_source_count)
            *out_source_count = loader.source_count;
    }
    else {

        for (int i = 0; i < loader.source_count; i++)
            free(loader.sources[i]);

        free(loader.sources);
    }

    for (int i = 0; i < loader.stack_count; i++)
        free(loader.stack[i]);

    free(loader.stack);

    for (int i = 0;
         i < loader.search_dir_count;
         i++)
    {
        free(loader.search_dirs[i]);
    }

    free(loader.search_dirs);

    free(loader.base_dir);

    return ok;
}


/* =========================================================
 * FICHEROS C EXTERNOS (external "ruta.c" fn ...)
 *
 * Se recogen del AST, se compilan con el mismo driver que
 * usa el linker y se añaden al enlace como objetos.
 * ========================================================= */

static int source_file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return 0;

    fclose(file);

    return 1;
}


static int source_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    if (path[0] == '/')
        return 1;

#ifdef _WIN32
    if (path[0] == '\\')
        return 1;

    if (path[1] == ':')
        return 1;
#endif

    return 0;
}


static char *source_join_path(
    const char *directory,
    const char *path
)
{
    if (directory == NULL)
        return string_duplicate(path);

    size_t directory_length = strlen(directory);
    size_t path_length = strlen(path);

    char *joined =
        malloc(directory_length + path_length + 2);

    if (joined == NULL)
        return NULL;

    memcpy(joined, directory, directory_length);
    joined[directory_length] = '/';
    memcpy(
        joined + directory_length + 1,
        path,
        path_length + 1
    );

    return joined;
}


/*
 * Ruta de un fichero C declarado con `external`:
 *
 *   - absoluta -> como está;
 *   - relativa -> junto al programa;
 *   - si no existe ahí -> relativa al directorio de
 *     trabajo;
 *   - si no existe en ninguno -> junto al programa, para
 *     que el error de compilación la muestre.
 */
static char *external_resolve_path(
    const char *base_dir,
    const char *path
)
{
    if (path == NULL)
        return NULL;

    if (source_path_is_absolute(path))
        return string_duplicate(path);

    char *joined =
        source_join_path(base_dir, path);

    if (joined != NULL && source_file_exists(joined))
        return joined;

    free(joined);

    if (source_file_exists(path))
        return string_duplicate(path);

    return source_join_path(base_dir, path);
}


static int string_list_add_unique(
    char ***list,
    int *count,
    const char *value
)
{
    if (value == NULL)
        return 0;

    for (int i = 0; i < *count; i++) {

        if ((*list)[i] != NULL &&
            strcmp((*list)[i], value) == 0)
            return 1;
    }

    char **grown =
        realloc(
            *list,
            (size_t)(*count + 1) * sizeof(char *)
        );

    if (grown == NULL)
        return 0;

    char *copy = string_duplicate(value);

    if (copy == NULL) {

        *list = grown;
        return 0;
    }

    *list = grown;
    (*list)[*count] = copy;
    (*count)++;

    return 1;
}


/*
 * Recorre el AST recogiendo las rutas de los ficheros C
 * declarados con `external`, también dentro de los módulos
 * importados. Las rutas repetidas se guardan una sola vez.
 */
static int external_collect_paths(
    ASTNode *node,
    const char *base_dir,
    char ***paths,
    int *count
)
{
    if (node == NULL)
        return 1;

    if (node->kind == AST_FUNCTION_DECL) {

        const char *declared =
            node->data.function_decl.external_path;

        if (declared == NULL)
            return 1;

        char *resolved =
            external_resolve_path(base_dir, declared);

        if (resolved == NULL)
            return 0;

        int ok =
            string_list_add_unique(
                paths,
                count,
                resolved
            );

        free(resolved);

        return ok;
    }

    if (node->kind == AST_IMPORT ||
        node->kind == AST_FROM_IMPORT)
    {
        ASTNodeList *declarations =
            import_node_declarations(node);

        if (declarations == NULL)
            return 1;

        for (int i = 0;
             i < declarations->count;
             i++) {

            if (!external_collect_paths(
                    declarations->items[i],
                    base_dir,
                    paths,
                    count
                ))
                return 0;
        }

        return 1;
    }

    return 1;
}


/* =========================================================
 * FFI: VERIFICACIÓN ABI (Fase 3.1)
 *
 * Genera un header C con las firmas esperadas de todas las
 * funciones `external` y lo fuerza con -include al compilar
 * cada fichero .c. Si el .c define una función con firma
 * distinta, clang emite "conflicting types" en compile-time.
 * ========================================================= */

/*
 * Mapa de tipo E++ a tipo C para la verificación ABI.
 * Devuelve NULL si el tipo no tiene representación C directa
 * (struct, list, map, etc. — no permitidos en FFI).
 */
static const char *ffi_c_type(const Type *type)
{
    if (type == NULL)
        return "void";

    switch (type->kind) {

        case TYPE_INT:     return "int64_t";
        case TYPE_FLOAT:   return "double";
        case TYPE_STRING:  return "const char *";
        case TYPE_BOOL:    return "unsigned char";
        case TYPE_CHAR:    return "char";

        case TYPE_I8:      return "int8_t";
        case TYPE_I16:     return "int16_t";
        case TYPE_I32:     return "int32_t";
        case TYPE_U8:      return "uint8_t";
        case TYPE_U16:     return "uint16_t";
        case TYPE_U32:     return "uint32_t";
        case TYPE_U64:     return "uint64_t";
        case TYPE_SIZE:    return "size_t";

        case TYPE_OPAQUE:  return "void *";

        /*
         * Structs POD se pasan como puntero opaco (void*).
         * El layout se verifica con _Static_assert en el
         * header generado.
         */
        case TYPE_STRUCT:  return "void *";

        /*
         * buffer<T> se expande en dos parámetros C:
         * (T*, int64_t). En el header se genera como
         * void* + int64_t (el T* real depende del
         * elemento).
         */
        case TYPE_BUFFER:  return "void *";

        default:
            return NULL;
    }
}


/*
 * Genera en `buffer` la declaración C de una función externa.
 * Para callbacks produce un puntero a función C.
 * Devuelve el número de caracteres escritos, o -1 si el
 * buffer no es suficiente o el tipo no es FFI-mapeable.
 */
static int ffi_write_signature(
    char *buffer,
    size_t size,
    const ASTNode *fn
)
{
    const char *ret =
        ffi_c_type(fn->data.function_decl.return_type);

    if (ret == NULL)
        return -1;

    int written = snprintf(
        buffer,
        size,
        "extern %s %s(",
        ret,
        fn->data.function_decl.name
    );

    if (written <= 0 || (size_t) written >= size)
        return -1;

    size_t length = (size_t) written;
    int count =
        fn->data.function_decl.parameters.count;

    if (count == 0) {

        if (length + 5 >= size)
            return -1;

        length += (size_t) snprintf(
            buffer + length,
            size - length,
            "void"
        );

    } else {

        int param_index = 0;

        for (int i = 0; i < count; i++) {

            ASTParameter *param =
                fn->data.function_decl.parameters.items[i];

            /*
             * buffer<T> se expande en dos parámetros C:
             * (void*, int64_t).
             */
            if (param->type != NULL &&
                param->type->kind == TYPE_BUFFER) {

                written = snprintf(
                    buffer + length,
                    size - length,
                    "%svoid *",
                    param_index > 0 ? ", " : ""
                );

                if (written <= 0 ||
                    (size_t) written >= size - length)
                    return -1;

                length += (size_t) written;
                param_index++;

                written = snprintf(
                    buffer + length,
                    size - length,
                    ", int64_t"
                );

                if (written <= 0 ||
                    (size_t) written >= size - length)
                    return -1;

                length += (size_t) written;
                param_index++;

                continue;
            }

            const char *c_type =
                ffi_c_type(param->type);

            if (c_type == NULL)
                return -1;

            /*
             * callback<T1,..> -> T  →  puntero a función C.
             */
            if (param->type != NULL &&
                param->type->kind == TYPE_CALLBACK) {

                const char *cb_ret =
                    ffi_c_type(param->type->return_type);

                if (cb_ret == NULL)
                    return -1;

                written = snprintf(
                    buffer + length,
                    size - length,
                    "%s%s (*",
                    param_index > 0 ? ", " : "",
                    cb_ret
                );

                if (written <= 0 ||
                    (size_t) written >= size - length)
                    return -1;

                length += (size_t) written;

                int cb_count =
                    param->type->parameter_count;

                if (length + 2 >= size)
                    return -1;

                buffer[length++] = '(';

                for (int j = 0; j < cb_count; j++) {

                    const char *cb_param =
                        ffi_c_type(
                            param->type->parameter_types[j]
                        );

                    if (cb_param == NULL)
                        return -1;

                    written = snprintf(
                        buffer + length,
                        size - length,
                        "%s%s",
                        j > 0 ? ", " : "",
                        cb_param
                    );

                    if (written <= 0 ||
                        (size_t) written >= size - length)
                        return -1;

                    length += (size_t) written;
                }

                if (length + 4 >= size)
                    return -1;

                length += (size_t) snprintf(
                    buffer + length,
                    size - length,
                    "))"
                );

            } else {

                written = snprintf(
                    buffer + length,
                    size - length,
                    "%s%s",
                    param_index > 0 ? ", " : "",
                    c_type
                );

                if (written <= 0 ||
                    (size_t) written >= size - length)
                    return -1;

                length += (size_t) written;
                param_index++;
            }
        }
    }

    if (length + 3 >= size)
        return -1;

    buffer[length++] = ')';
    buffer[length++] = ';';
    buffer[length] = '\0';

    return (int) length;
}


/*
 * Recorre el AST generando en `buffer` las declaraciones C de
 * todas las funciones `external` apuntando a `path`.
 * Devuelve 1 si al menos una función se escribió.
 */
static int ffi_write_declarations(
    ASTNode *node,
    const char *path,
    char *buffer,
    size_t size,
    size_t *length
)
{
    if (node == NULL)
        return 1;

    if (node->kind == AST_FUNCTION_DECL) {

        const char *declared =
            node->data.function_decl.external_path;

        if (declared == NULL)
            return 1;

        /*
         * Comparar por nombre de fichero final: la ruta
         * declarada puede ser relativa, pero `path` ya está
         * resuelta. Usamos strstr como comprobación barata.
         */
        const char *base = strrchr(declared, '/');
        base = base != NULL ? base + 1 : declared;

        if (strstr(path, base) == NULL)
            return 1;

        char signature[1024];

        int sig_len = ffi_write_signature(
            signature,
            sizeof(signature),
            node
        );

        if (sig_len < 0)
            return 1;

        if (*length + (size_t) sig_len + 2 >= size)
            return 0;

        buffer[*length] = '\n';
        (*length)++;
        memcpy(
            buffer + *length,
            signature,
            (size_t) sig_len
        );
        *length += (size_t) sig_len;
        buffer[*length] = '\0';

        return 1;
    }

    if (node->kind == AST_IMPORT ||
        node->kind == AST_FROM_IMPORT)
    {
        ASTNodeList *declarations =
            import_node_declarations(node);

        if (declarations == NULL)
            return 1;

        for (int i = 0;
             i < declarations->count;
             i++) {

            if (!ffi_write_declarations(
                    declarations->items[i],
                    path,
                    buffer,
                    size,
                    length
                ))
                return 0;
        }

        return 1;
    }

    return 1;
}


/*
 * Escribe en disco el header de verificación ABI para un
 * fichero C externo. Devuelve la ruta del header (malloc) o
 * NULL si no hay funciones que verificar o falla la escritura.
 */
static char *ffi_generate_header(
    ASTNode *program,
    const char *c_path,
    const char *output_base
)
{
    char *content = malloc(65536);

    if (content == NULL)
        return NULL;

    size_t header_end = (size_t) snprintf(
        content,
        65536,
        "/* Generado por rivo — verificación ABI. NO editar. */\n"
        "#ifndef RIVO_FFI_CHECK_H\n"
        "#define RIVO_FFI_CHECK_H\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n"
    );

    size_t length = header_end;

    int ok = 1;

    for (int i = 0;
         ok &&
         i < program->data.program.declarations.count;
         i++)
    {
        ok = ffi_write_declarations(
            program->data.program.declarations.items[i],
            c_path,
            content,
            65536,
            &length
        );
    }

    if (!ok || length <= header_end) {

        /* Sin funciones externas para este fichero. */
        free(content);
        return NULL;
    }

    if (length + 32 >= 65536) {
        free(content);
        return NULL;
    }

    length += (size_t) snprintf(
        content + length,
        65536 - length,
        "\n#endif\n"
    );

    char *header_path = malloc(8192);

    if (header_path == NULL) {
        free(content);
        return NULL;
    }

    snprintf(
        header_path,
        8192,
        "%s.rivo_ffi.h",
        output_base
    );

    FILE *file = fopen(header_path, "w");

    if (file == NULL) {
        free(header_path);
        free(content);
        return NULL;
    }

    fwrite(content, 1, length, file);
    fclose(file);
    free(content);

    return header_path;
}


/*
 * Compila un fichero C a un objeto con el mismo driver que
 * usa el linker. Las salidas del compilador (en inglés)
 * salen por su canal normal; aquí se añade el aviso en
 * español.
 *
 * Se pasan a clang el directorio del programa, los de -I y
 * el de la biblioteca estándar, para que los `#include`
 * de esos ficheros C se resuelvan.
 */
static int external_append_include(
    char *command,
    size_t size,
    size_t *length,
    const char *directory
)
{
    if (directory == NULL ||
        directory[0] == '\0')
        return 1;

    int written = snprintf(
        command + *length,
        size - *length,
        " -I \"%s\"",
        directory
    );

    if (written <= 0 ||
        (size_t) written >= size - *length)
        return 0;

    *length += (size_t) written;

    return 1;
}


static int external_compile_source(
    const char *source,
    const char *object,
    const char *include_dir,
    const CompilerOptions *options,
    const char *ffi_header
)
{
    char command[4096];

    int written = snprintf(
        command,
        sizeof(command),
        "%s -c \"%s\" -o \"%s\"",
        linker_compiler(),
        source,
        object
    );

    if (written <= 0 || (size_t) written >= sizeof(command))
        return 0;

    size_t length = (size_t) written;

    /*
     * FFI: forzar la inclusión del header de verificación
     * ABI. Si el .c define una función con firma distinta,
     * clang emite "conflicting types" en compile-time.
     */
    if (ffi_header != NULL) {

        written = snprintf(
            command + length,
            sizeof(command) - length,
            " -include \"%s\"",
            ffi_header
        );

        if (written <= 0 ||
            (size_t) written >= sizeof(command) - length)
            return 0;

        length += (size_t) written;
    }

    int ok =
        external_append_include(
            command,
            sizeof(command),
            &length,
            include_dir
        );

    if (ok && options != NULL) {

        for (int i = 0;
             i < options->include_count && ok;
             i++)
        {
            ok = external_append_include(
                command,
                sizeof(command),
                &length,
                options->include_dirs[i]
            );
        }
    }

#ifdef RIVO_STDLIB_PATH
    if (ok) {

        ok = external_append_include(
            command,
            sizeof(command),
            &length,
            rivo_stdlib_path()
        );
    }
#endif

    if (!ok)
        return 0;

    int status = system(command);

    if (status != 0)
        return 0;

    /* El objeto tiene que existir tras un 0 del driver. */
    return source_file_exists(object);
}


/* =========================================================
 * BUILD: ESTADO Y EJECUCIÓN
 * ========================================================= */

static void build_state_destroy(
    BuildPaths *paths,
    char *project_dir,
    char *manifest_path,
    char *project_name,
    char *project_version,
    char *binary_name
)
{
    build_paths_destroy(paths);

    free(project_dir);
    free(manifest_path);
    free(project_name);
    free(project_version);
    free(binary_name);
}


/*
 * Ejecuta el binario compilado (--run). Los argumentos
 * tras `--` se pasan al programa. Devuelve su código de
 * salida normalizado.
 */
static int run_compiled_binary(
    const CompilerOptions *options
)
{
    if (!options || !options->output_file)
        return 1;

    const char *output =
        options->output_file;

    build_status_running(output);

    /*
     * En POSIX, un path sin '/' no se busca en el CWD:
     * hay que anteponer "./".
     */
    const char *prefix = "";

#ifdef _WIN32
    if (!strchr(output, '/') && !strchr(output, '\\'))
        prefix = ".\\";
#else
    if (!strchr(output, '/'))
        prefix = "./";
#endif

    size_t length =
        strlen(prefix) + strlen(output) + 1;

    for (int i = 0; i < options->run_arg_count; i++)
    {
        if (options->run_args[i])
            length += 1 + strlen(options->run_args[i]);
    }

    char *command =
        malloc(length + 1);

    if (!command)
        return 1;

    char *cursor = command;

    cursor += sprintf(cursor, "%s%s", prefix, output);

    for (int i = 0; i < options->run_arg_count; i++)
    {
        if (options->run_args[i])
            cursor += sprintf(
                cursor,
                " %s",
                options->run_args[i]
            );
    }

    int status =
        system(command);

    free(command);

    if (status < 0)
        return 1;

#ifdef _WIN32
    return status;
#else
    if (status == 0)
        return 0;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
#endif
}


/* =========================================================
 * MAIN
 * ========================================================= */

int main(
    int argc,
    char **argv
)
{
    CompilerOptions options;

    /*
     * -----------------------------------------------------
     * SUBCOMANDOS DEL GESTOR DE PAQUETES
     * -----------------------------------------------------
     *
     * `rivo install`, `rivo add`, ... se atienden antes
     * del parser de flags. El compilador sigue igual:
     * `rivo archivo.ro -o salida`.
     */

    if (argc >= 2 &&
        strcmp(argv[1], "fmt") == 0)
    {
        return formatter_run(argc, argv);
    }

    if (argc >= 2 &&
        argv[1][0] != '-' &&
        packages_is_command(argv[1]))
    {
        return packages_run(argc - 1, argv + 1);
    }

    /*
     * -----------------------------------------------------
     * ARGUMENTOS
     * -----------------------------------------------------
     */

    if (!parse_arguments(
            argc,
            argv,
            &options))
    {
        return 1;
    }


    /*
     * -----------------------------------------------------
     * MANIFIESTO rivo.toml
     * -----------------------------------------------------
     *
     * Si existe un manifiesto de proyecto subiendo desde
     * el directorio del archivo de entrada (o desde el
     * directorio de trabajo), se fusiona con las opciones.
     * Los flags de la línea de comandos siempre tienen
     * prioridad sobre el manifiesto.
     */

    char *manifest_entry = NULL;

    ProjectManifest *manifest = NULL;

    /*
     * Estado de build (target/) copiado del manifiesto
     * antes de destruirlo.
     */
    int has_manifest_build = 0;
    int warnings_as_errors = 0;

    BuildProfile build_profile =
        BUILD_PROFILE_DEBUG;

    char *build_project_dir = NULL;
    char *build_manifest_path = NULL;
    char *build_project_name = NULL;
    char *build_project_version = NULL;
    char *build_binary_name = NULL;

    double build_start = build_now();

    char *manifest_start =
        options.input_file
            ? program_directory(options.input_file)
            : string_duplicate(".");

    if (!manifest_start)
    {
        fprintf(
            stderr,
            "rivo: memoria insuficiente\n"
        );

        return 1;
    }

    char manifest_error[512];

    ManifestStatus manifest_status =
        manifest_load(
            manifest_start,
            &manifest,
            manifest_error,
            sizeof(manifest_error)
        );

    free(manifest_start);

    if (manifest_status == MANIFEST_INVALID)
    {
        fprintf(
            stderr,
            "rivo: %s\n",
            manifest_error
        );

        return 1;
    }

    if (manifest_status == MANIFEST_OK)
    {
        verbose_stage(
            &options,
            "usando manifiesto rivo.toml"
        );

        /*
         * Archivo de entrada declarado en el manifiesto:
         * solo se usa si la línea de comandos no dio uno.
         */
        if (!options.input_file && manifest->entrada)
        {
            if (source_path_is_absolute(manifest->entrada))
            {
                manifest_entry =
                    string_duplicate(manifest->entrada);
            }
            else
            {
                manifest_entry =
                    source_join_path(
                        manifest->dir,
                        manifest->entrada
                    );
            }

            if (!manifest_entry ||
                !source_file_exists(manifest_entry))
            {
                fprintf(
                    stderr,
                    "rivo: el archivo de entrada '%s' "
                    "del manifiesto no existe\n",
                    manifest_entry
                        ? manifest_entry
                        : manifest->entrada
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }

            options.input_file = manifest_entry;
        }

        /*
         * Directorios de [dependencias]: se añaden a la
         * búsqueda de módulos, igual que -I. Las rutas
         * relativas se resuelven junto al manifiesto.
         */
        for (int i = 0;
             i < manifest->dep_count;
             i++)
        {
            const char *dependency =
                manifest->dep_dirs[i];

            char *resolved;

            if (source_path_is_absolute(dependency))
            {
                resolved =
                    string_duplicate(dependency);
            }
            else
            {
                resolved =
                    source_join_path(
                        manifest->dir,
                        dependency
                    );
            }

            if (!resolved)
            {
                fprintf(
                    stderr,
                    "rivo: memoria insuficiente\n"
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }

            char **include_dirs =
                (char **)options.include_dirs;

            int added =
                string_list_add_unique(
                    &include_dirs,
                    &options.include_count,
                    resolved
                );

            options.include_dirs =
                (const char **)include_dirs;

            free(resolved);

            if (!added)
            {
                fprintf(
                    stderr,
                    "rivo: memoria insuficiente\n"
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }
        }

        /*
         * rivo.lock: las dependencias resueltas por
         * `rivo install` se anaden a la busqueda de modulos.
         * Las de version sin resolver son un error claro.
         */
        {
            char lock_path[PATH_MAX];

            int written =
                snprintf(
                    lock_path,
                    sizeof(lock_path),
                    "%s/%s",
                    manifest->dir,
                    RIVO_LOCK_FILENAME
                );

            LockFile *lock =
                (written > 0 &&
                 (size_t) written < sizeof(lock_path))
                    ? lockfile_load(lock_path)
                    : NULL;

            for (int i = 0;
                 i < manifest->dependency_count;
                 i++)
            {
                ManifestDependency *dependency =
                    &manifest->dependencies[i];

                if (dependency->kind !=
                        MANIFEST_DEP_VERSION)
                    continue;

                if (lock == NULL ||
                    lockfile_find(lock, dependency->name)
                        == NULL)
                {
                    fprintf(
                        stderr,
                        "rivo: la dependencia '%s' (%s) no "
                        "esta resuelta; ejecuta `rivo install`\n",
                        dependency->name,
                        dependency->requirement
                    );

                    lockfile_destroy(lock);
                    free(manifest_entry);
                    manifest_destroy(manifest);

                    return 1;
                }
            }

            if (lock != NULL)
            {
                for (int i = 0; i < lock->count; i++)
                {
                    const char *source =
                        lock->packages[i].source;

                    if (source == NULL ||
                        strncmp(source, "ruta:", 5) != 0)
                        continue;

                    /*
                     * El lock guarda rutas relativas al
                     * manifiesto; se resuelven con su dir
                     * para compilar desde cualquier cwd.
                     */
                    char *resolved =
                        source_path_is_absolute(source + 5)
                            ? string_duplicate(source + 5)
                            : source_join_path(
                                  manifest->dir,
                                  source + 5);

                    if (resolved == NULL)
                    {
                        fprintf(
                            stderr,
                            "rivo: memoria insuficiente\n"
                        );

                        lockfile_destroy(lock);
                        free(manifest_entry);
                        manifest_destroy(manifest);

                        return 1;
                    }

                    char **include_dirs =
                        (char **)options.include_dirs;

                    int added =
                        string_list_add_unique(
                            &include_dirs,
                            &options.include_count,
                            resolved
                        );

                    options.include_dirs =
                        (const char **)include_dirs;

                    free(resolved);

                    if (!added)
                    {
                        fprintf(
                            stderr,
                            "rivo: memoria insuficiente\n"
                        );

                        lockfile_destroy(lock);
                        free(manifest_entry);
                        manifest_destroy(manifest);

                        return 1;
                    }
                }
            }

            lockfile_destroy(lock);
        }

        /*
         * Opciones del compilador: solo se aplican si el
         * usuario no las dio en la línea de comandos.
         */
        if (!options.optimize_set &&
            manifest->optimizar)
        {
            if (strcmp(manifest->optimizar, "0") == 0)
                options.optimize = 0;
            else if (strcmp(manifest->optimizar, "1") == 0)
                options.optimize = 1;
            else if (strcmp(manifest->optimizar, "2") == 0)
                options.optimize = 2;
            else if (strcmp(manifest->optimizar, "3") == 0)
                options.optimize = 3;
            else
            {
                fprintf(
                    stderr,
                    "rivo: 'optimizar' debe ser \"0\", "
                    "\"1\", \"2\" o \"3\" en %s\n",
                    manifest->ruta
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }

            options.optimize_set = 1;
        }

        if (!options.debug_set &&
            manifest->depurar)
        {
            if (strcmp(manifest->depurar, "true") == 0)
                options.debug_info = 1;
            else if (strcmp(manifest->depurar, "false") == 0)
                options.debug_info = 0;
            else
            {
                fprintf(
                    stderr,
                    "rivo: 'depurar' debe ser \"true\" o "
                    "\"false\" en %s\n",
                    manifest->ruta
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }

            options.debug_set = 1;
        }

        /*
         * Perfil de build: la carpeta de target/. Si el
         * usuario no dio -O*, un perfil explícito del
         * manifiesto también fija el nivel de optimización
         * por defecto.
         */
        if (!options.optimize_set &&
            manifest->perfil)
        {
            if (strcmp(manifest->perfil, "debug") != 0 &&
                strcmp(manifest->perfil, "release") != 0)
            {
                fprintf(
                    stderr,
                    "rivo: 'perfil' debe ser \"debug\" o "
                    "\"release\" en %s\n",
                    manifest->ruta
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }

            int release =
                strcmp(manifest->perfil, "release") == 0;

            options.optimize =
                release ? 2 : 0;
            options.optimize_set = 1;

            build_profile =
                release
                    ? BUILD_PROFILE_RELEASE
                    : BUILD_PROFILE_DEBUG;
        }
        else
        {
            build_profile =
                build_profile_from_optimize(
                    options.optimize
                );
        }

        /*
         * Advertencias como errores.
         */
        if (manifest->advertencias_como_errores)
        {
            if (strcmp(
                    manifest->advertencias_como_errores,
                    "true") == 0)
            {
                warnings_as_errors = 1;
            }
            else if (strcmp(
                    manifest->advertencias_como_errores,
                    "false") != 0)
            {
                fprintf(
                    stderr,
                    "rivo: 'advertencias_como_errores' debe "
                    "ser \"true\" o \"false\" en %s\n",
                    manifest->ruta
                );

                free(manifest_entry);
                manifest_destroy(manifest);

                return 1;
            }
        }

        /*
         * Datos que siguen haciendo falta tras destruir el
         * manifiesto (build, caché, estado).
         */
        if (manifest->dir)
            build_project_dir =
                string_duplicate(manifest->dir);

        if (manifest->ruta)
            build_manifest_path =
                string_duplicate(manifest->ruta);

        if (manifest->nombre)
            build_project_name =
                string_duplicate(manifest->nombre);

        if (manifest->version)
            build_project_version =
                string_duplicate(manifest->version);

        if (manifest->binario)
            build_binary_name =
                string_duplicate(manifest->binario);

        has_manifest_build = 1;

        if (options.verbose &&
            (manifest->nombre || manifest->version))
        {
            printf(
                "[rivo] proyecto: %s %s\n",
                manifest->nombre
                    ? manifest->nombre
                    : "(sin nombre)",
                manifest->version
                    ? manifest->version
                    : ""
            );
        }

        manifest_destroy(manifest);
        manifest = NULL;
    }

    if (!options.input_file)
    {
        fprintf(
            stderr,
            "rivo: falta el archivo de entrada\n\n"
        );

        print_usage(argv[0]);

        free(manifest_entry);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * BUILD (target/ + caché + estado)
     * -----------------------------------------------------
     *
     * Solo con manifiesto y sin -o explícito: los
     * artefactos van a target/<perfil>/ del proyecto.
     */

    BuildPaths *build_paths = NULL;

    if (has_manifest_build && !options.output_file)
    {
        if (!build_binary_name &&
            build_project_name)
        {
            build_binary_name =
                string_duplicate(build_project_name);
        }

        if (!build_binary_name)
        {
            build_binary_name =
                default_output_name(
                    options.input_file
                );
        }

#ifdef _WIN32
        /*
         * En Windows el ejecutable lleva .exe.
         */
        if (build_binary_name)
        {
            size_t name_length =
                strlen(build_binary_name);

            int has_exe =
                name_length >= 4 &&
                strcmp(
                    build_binary_name + name_length - 4,
                    ".exe") == 0;

            if (!has_exe)
            {
                char *with_exe =
                    malloc(name_length + 5);

                if (with_exe)
                {
                    sprintf(
                        with_exe,
                        "%s.exe",
                        build_binary_name
                    );

                    free(build_binary_name);
                    build_binary_name = with_exe;
                }
            }
        }
#endif

        if (!build_binary_name || !build_project_dir)
        {
            fprintf(
                stderr,
                "rivo: memoria insuficiente\n"
            );

            return 1;
        }

        build_paths =
            build_paths_create(
                build_project_dir,
                build_profile,
                build_binary_name
            );

        if (!build_paths ||
            !build_ensure_dir(build_paths->dir))
        {
            fprintf(
                stderr,
                "rivo: no se pudo crear '%s'\n",
                build_paths
                    ? build_paths->dir
                    : "target"
            );

            build_paths_destroy(build_paths);

            return 1;
        }

        /*
         * En proyectos con manifiesto, el ejecutable y el
         * objeto van a target/<perfil>/.
         */
        options.output_file =
            build_paths->binary;

        options.object_file =
            build_paths->object;

        /*
         * Caché por fecha: solo para builds completos. Si
         * nada cambió, se reutiliza el binario.
         */
        int full_build =
            !options.emit_ir &&
            !options.emit_assembly &&
            !options.emit_object;

        if (full_build &&
            build_cache_is_fresh(
                build_paths,
                build_manifest_path,
                options.input_file,
                options.optimize))
        {
            build_status_finished(
                build_profile_name(build_profile),
                build_now() - build_start,
                1
            );

            int exit_code = 0;

            if (options.run_after)
                exit_code =
                    run_compiled_binary(&options);

            build_state_destroy(
                build_paths,
                build_project_dir,
                build_manifest_path,
                build_project_name,
                build_project_version,
                build_binary_name
            );

            free(manifest_entry);

            return exit_code;
        }

        build_status_compiling(
            build_project_name,
            build_project_version,
            build_project_dir
        );
    }


    /*
     * -----------------------------------------------------
     * NOMBRES DE ARCHIVOS
     * -----------------------------------------------------
     */

    char *generated_output = NULL;

    if (!options.output_file)
    {
        generated_output =
            default_output_name(
                options.input_file
            );

        if (!generated_output)
        {
            fprintf(
                stderr,
                "rivo: no se pudo determinar el archivo de salida\n"
            );

            return 1;
        }

        options.output_file =
            generated_output;
    }


    /*
     * Archivo objeto temporal.
     */

    char *generated_object = NULL;

    if (!options.object_file)
    {
        const char *output =
            options.output_file;

        size_t length =
            strlen(output);

        generated_object =
            malloc(length + 4 + 1);

        if (!generated_object)
        {
            free(generated_output);

            fprintf(
                stderr,
                "rivo: memoria insuficiente\n"
            );

            return 1;
        }

        snprintf(
            generated_object,
            length + 5,
            "%s.o",
            output
        );

        options.object_file =
            generated_object;
    }


    /*
     * -----------------------------------------------------
     * LEER FUENTE
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "leyendo archivo fuente"
    );

    size_t source_size = 0;

    char *source =
        read_file(
            options.input_file,
            &source_size
        );

    if (!source)
    {
        free(generated_output);
        free(generated_object);

        return 1;
    }


    if (options.verbose)
    {
        printf(
            "[rivo] archivo: %s\n",
            options.input_file
        );

        printf(
            "[rivo] tamaño: %zu bytes\n",
            source_size
        );
    }


    /*
     * -----------------------------------------------------
     * CONTEXTO DE ERRORES
     * -----------------------------------------------------
     */

    ErrorContext *errors =
        error_context_create(
            options.input_file,
            source
        );

    if (!errors)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el contexto de errores\n"
        );

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * Limitar cantidad de errores.
     */

    error_set_max_errors(
        errors,
        100
    );

    if (warnings_as_errors)
        error_set_warnings_as_errors(errors, 1);


    /*
     * -----------------------------------------------------
     * LEXER
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "iniciando lexer"
    );

    double time_analysis =
        build_now();

    Lexer *lexer =
        lexer_create(source);

    lexer_set_filename(
        lexer,
        options.input_file
    );

    if (!lexer)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el lexer\n"
        );

        error_report_simple(
            errors,
            ERROR_INTERNAL_OUT_OF_MEMORY,
            ERROR_PHASE_INTERNAL,
            0,
            0,
            "no se pudo crear el lexer"
        );

        error_print_all(errors);

        error_context_destroy(errors);
        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * PARSER
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "analizando sintaxis"
    );

    Parser *parser =
        parser_create(lexer);

    if (!parser)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el parser\n"
        );

        lexer_destroy(lexer);
        error_context_destroy(errors);
        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    ASTNode *program =
        parser_parse(parser);

    /*
     * Nota:
     *
     * El parser actual tiene su propio had_error.
     *
     * Cuando un parser nuevo use ErrorContext,
     * ambos sistemas quedarán completamente unificados.
     */

    if (!program ||
        parser_has_error(parser))
    {
        error_print_all(errors);

        if (program)
            ast_destroy(program);

        parser_destroy(parser);
        lexer_destroy(lexer);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * Ya no necesitamos lexer/parser.
     */

    parser_destroy(parser);
    lexer_destroy(lexer);


    /*
     * -----------------------------------------------------
     * MÓDULOS (import)
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "resolviendo imports"
    );

    double time_imports = build_now();

    if (options.verbose)
        build_phase_timing(
            "análisis",
            time_analysis,
            time_imports
        );

    char **module_sources = NULL;
    int module_source_count = 0;

    if (!load_program_modules(
            program,
            options.input_file,
            &options,
            errors,
            &module_sources,
            &module_source_count))
    {
        error_print_all(errors);

        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        for (int i = 0; i < module_source_count; i++)
            free(module_sources[i]);

        free(module_sources);

        return 1;
    }

    if (options.verbose)
        build_phase_timing("imports", time_imports, build_now());


    /*
     * -----------------------------------------------------
     * SEMÁNTICA
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "analizando tipos y semántica"
    );

    double time_semantic =
        build_now();

    SemanticContext *semantic =
        semantic_create();

    if (!semantic)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el contexto semántico\n"
        );

        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    int semantic_valid =
        semantic_analyze(
            semantic,
            program);

    int semantic_warnings = 0;

    /*
     * El sistema semantic.h actual mantiene sus propios
     * errores y avisos.
     */
    for (int i = 0;
         i < semantic_error_count(semantic);
         i++)
    {
        const SemanticError *error =
            semantic_get_error(
                semantic,
                i
            );

        if (error)
        {
            if (error->is_warning)
                semantic_warnings++;

            error_render(
                semantic_error_class_name(
                    error->kind
                ),
                error->is_warning
                    ? ERROR_WARNING
                    : ERROR_ERROR,
                options.input_file,
                source,
                error->line,
                error->column,
                1,
                error->message
                    ? error->message
                    : (error->is_warning
                           ? "aviso semántico"
                           : "error semántico"),
                NULL,
                error->suggestion
            );
        }
    }

    if (!semantic_valid ||
        (warnings_as_errors && semantic_warnings > 0))
    {
        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * LLVM
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "iniciando LLVM"
    );

    if (options.verbose)
        build_phase_timing(
            "semántica",
            time_semantic,
            build_now()
        );

    if (!llvm_initialize())
    {
        fprintf(
            stderr,
            "rivo: no se pudo inicializar LLVM\n"
        );

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    LLVMConfig llvm_config =
        llvm_default_config();

    llvm_config.module_name =
        options.input_file;

    llvm_config.debug_info =
        options.debug_info;

    llvm_config.optimization_level =
        options.optimize;

    llvm_config.output_type =
        LLVM_OUTPUT_OBJECT;


    /*
     * Crear compilador LLVM.
     */

    LLVMCompiler *llvm =
        llvm_create(
            &llvm_config,
            errors
        );

    if (!llvm)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el compilador LLVM\n"
        );

        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * GENERACIÓN LLVM
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "generando código LLVM"
    );

    double time_codegen =
        build_now();

    if (!llvm_generate(
            llvm,
            program))
    {
        error_print_all(errors);

        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * VERIFICAR LLVM
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "verificando módulo LLVM"
    );

    if (!llvm_verify(llvm))
    {
        fprintf(
            stderr,
            "rivo: el módulo LLVM no es válido\n"
        );

        error_print_all(errors);

        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * OPTIMIZAR
     * -----------------------------------------------------
     */

    if (options.optimize > 0)
    {
        verbose_stage(
            &options,
            "optimizando código"
        );

        if (!llvm_optimize(llvm))
        {
            fprintf(
                stderr,
                "rivo: error durante la optimización\n"
            );

            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }
    }


    /*
     * -----------------------------------------------------
     * LLVM IR
     * -----------------------------------------------------
     */

    if (options.emit_ir)
    {
        verbose_stage(
            &options,
            "escribiendo LLVM IR"
        );

        size_t output_length =
            strlen(options.output_file);

        char *ir_file =
            malloc(
                output_length + 4
            );

        if (!ir_file)
        {
            fprintf(
                stderr,
                "rivo: memoria insuficiente\n"
            );

            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }

        snprintf(
            ir_file,
            output_length + 4,
            "%s.ll",
            options.output_file
        );

        if (!llvm_write_ir(
                llvm,
                ir_file))
        {
            fprintf(
                stderr,
                "rivo: no se pudo escribir '%s'\n",
                ir_file
            );

            free(ir_file);

            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }

        printf(
            "IR generado: %s\n",
            ir_file
        );

        free(ir_file);

        /*
         * Si solo queremos IR, terminamos aquí.
         */
        if (!options.emit_object &&
            !options.emit_assembly)
        {
            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 0;
        }
    }


    /*
     * -----------------------------------------------------
     * ASSEMBLY
     * -----------------------------------------------------
     */

    if (options.emit_assembly)
    {
        verbose_stage(
            &options,
            "escribiendo ensamblador"
        );

        size_t output_length =
            strlen(options.output_file);

        char *assembly_file =
            malloc(
                output_length + 5
            );

        if (!assembly_file)
        {
            fprintf(
                stderr,
                "rivo: memoria insuficiente\n"
            );

            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }

        snprintf(
            assembly_file,
            output_length + 5,
            "%s.s",
            options.output_file
        );

        if (!llvm_write_assembly(
                llvm,
                assembly_file))
        {
            fprintf(
                stderr,
                "rivo: no se pudo escribir '%s'\n",
                assembly_file
            );

            free(assembly_file);

            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }

        printf(
            "Assembly generado: %s\n",
            assembly_file
        );

        free(assembly_file);

        /*
         * Si solo queremos assembly, terminamos.
         */
        if (!options.emit_object)
        {
            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 0;
        }
    }


    if (options.verbose)
        build_phase_timing(
            "código",
            time_codegen,
            build_now()
        );


    /*
     * -----------------------------------------------------
     * OBJETO
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "generando archivo objeto"
    );

    double time_object =
        build_now();

    if (!llvm_write_object(
            llvm,
            options.object_file))
    {
        fprintf(
            stderr,
            "rivo: no se pudo generar '%s'\n",
            options.object_file
        );

        error_print_all(errors);

        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }

    if (options.verbose)
        build_phase_timing(
            "objeto",
            time_object,
            build_now()
        );

    if (!has_manifest_build)
        printf(
            "Objeto generado: %s\n",
            options.object_file
        );


    /*
     * Si se pidió explícitamente -c,
     * no hacemos linking.
     */

    if (options.emit_object)
    {
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 0;
    }


    /*
     * -----------------------------------------------------
     * LINKER
     * -----------------------------------------------------
     */

    verbose_stage(
        &options,
        "iniciando linker"
    );

    LinkerConfig linker_config =
        linker_default_config();

    linker_config.debug_info =
        options.debug_info;

    linker_config.optimize =
        options.optimize;


    Linker *linker =
        linker_create(
            &linker_config,
            errors
        );

    if (!linker)
    {
        fprintf(
            stderr,
            "rivo: no se pudo crear el linker\n"
        );

        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * Bibliotecas del usuario (-L y -l).
     */

    for (int i = 0;
         i < options.library_path_count;
         i++)
    {
        if (!linker_add_library_path(
                linker,
                options.library_paths[i]))
        {
            fprintf(
                stderr,
                "rivo: no se pudo anadir la ruta de "
                "bibliotecas \"%s\"\n",
                options.library_paths[i]
            );

            linker_destroy(linker);
            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }
    }

    for (int i = 0;
         i < options.library_count;
         i++)
    {
        if (!linker_add_library(
                linker,
                options.libraries[i]))
        {
            fprintf(
                stderr,
                "rivo: no se pudo anadir la biblioteca "
                "\"%s\"\n",
                options.libraries[i]
            );

            linker_destroy(linker);
            llvm_destroy(llvm);
            llvm_shutdown();

            semantic_destroy(semantic);
            ast_destroy(program);
            error_context_destroy(errors);

            free(source);
            free(generated_output);
            free(generated_object);

            return 1;
        }
    }


    /*
     * Archivo objeto.
     */

    if (!linker_set_object_file(
            linker,
            options.object_file))
    {
        fprintf(
            stderr,
            "rivo: no se pudo configurar el archivo objeto\n"
        );

        linker_destroy(linker);
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * Archivo ejecutable.
     */

    if (!linker_set_output_file(
            linker,
            options.output_file))
    {
        fprintf(
            stderr,
            "rivo: no se pudo configurar el archivo de salida\n"
        );

        linker_destroy(linker);
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * Runtime de E++.
     *
     * Los programas que usan print(), input() o len() se
     * enlazan contra el archivo de runtime del sistema. La ruta la define
     * CMake con RIVO_RUNTIME_PATH; si el proyecto se
     * compila sin CMake no se activa y el enlace fallará
     * con los símbolos runtime sin resolver.
     */

#ifdef RIVO_RUNTIME_PATH

    if (!linker_set_runtime(linker, RIVO_RUNTIME_PATH))
    {
        fprintf(
            stderr,
            "rivo: no se pudo configurar el runtime (%s)\n",
            RIVO_RUNTIME_PATH
        );

        linker_destroy(linker);
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }

#endif


    /*
     * -----------------------------------------------------
     * FICHEROS C EXTERNOS
     *
     * `external "ruta.c" fn ...`: se compilan aquí y se
     * añaden al enlace como objetos.
     * -----------------------------------------------------
     */

    char **external_sources = NULL;
    int external_source_count = 0;

    char **external_objects = NULL;
    int external_object_count = 0;

    char *program_dir =
        program_directory(options.input_file);

    int externals_ok = 1;

    if (program_dir == NULL) {

        fprintf(
            stderr,
            "rivo: no se pudo resolver el directorio "
            "del programa\n"
        );

        externals_ok = 0;
    }

    for (int i = 0;
         externals_ok &&
         i < program->data.program.declarations.count;
         i++)
    {
        externals_ok =
            external_collect_paths(
                program->data.program.declarations.items[i],
                program_dir,
                &external_sources,
                &external_source_count
            );
    }

    if (externals_ok) {

        for (int i = 0;
             i < external_source_count;
             i++)
        {
            char object_file[4096];

            snprintf(
                object_file,
                sizeof(object_file),
                "%s.ext%d.o",
                options.object_file,
                i
            );

            /*
             * FFI: generar header de verificación ABI y
             * forzarlo con -include (solo con --ffi-check).
             */
            char *ffi_header = NULL;

            if (options.ffi_check) {

                ffi_header = ffi_generate_header(
                    program,
                    external_sources[i],
                    object_file
                );
            }

            int compiled =
                external_compile_source(
                    external_sources[i],
                    object_file,
                    program_dir,
                    &options,
                    ffi_header
                );

            free(ffi_header);

            if (!compiled)
            {
                fprintf(
                    stderr,
                    "rivo: no se pudo compilar el fichero C "
                    "\"%s\"\n",
                    external_sources[i]
                );

                externals_ok = 0;
                break;
            }

            if (!linker_add_library(
                    linker,
                    object_file))
            {
                fprintf(
                    stderr,
                    "rivo: no se pudo anadir al enlace el "
                    "objeto de \"%s\"\n",
                    external_sources[i]
                );

                externals_ok = 0;
                break;
            }

            if (!module_list_add(
                    &external_objects,
                    &external_object_count,
                    object_file))
            {
                externals_ok = 0;
                break;
            }

            if (options.verbose)
            {
                printf(
                    "[rivo] fichero C externo: %s -> %s\n",
                    external_sources[i],
                    object_file
                );
            }
        }
    }

    if (!externals_ok)
    {
        for (int i = 0; i < external_object_count; i++)
            free(external_objects[i]);

        free(external_objects);

        for (int i = 0; i < external_source_count; i++)
            free(external_sources[i]);

        free(external_sources);
        free(program_dir);

        linker_destroy(linker);
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * LINK
     * -----------------------------------------------------
     */

    if (options.verbose)
    {
        printf(
            "[rivo] objeto: %s\n",
            options.object_file
        );

        printf(
            "[rivo] salida: %s\n",
            options.output_file
        );
    }


    if (has_manifest_build)
    {
        build_status_linking(
            build_profile_name(build_profile)
        );
    }

    double time_link =
        build_now();

    if (!linker_link(linker))
    {
        fprintf(
            stderr,
            "rivo: error enlazando el ejecutable\n"
        );

        if (linker_get_command(linker))
        {
            fprintf(
                stderr,
                "rivo: comando:\n"
                "  %s\n",
                linker_get_command(linker)
            );
        }

        error_print_all(errors);

        for (int i = 0; i < external_object_count; i++)
            free(external_objects[i]);

        free(external_objects);

        for (int i = 0; i < external_source_count; i++)
            free(external_sources[i]);

        free(external_sources);
        free(program_dir);

        linker_destroy(linker);
        llvm_destroy(llvm);
        llvm_shutdown();

        semantic_destroy(semantic);
        ast_destroy(program);
        error_context_destroy(errors);

        free(source);
        free(generated_output);
        free(generated_object);

        return 1;
    }


    /*
     * -----------------------------------------------------
     * ÉXITO
     * -----------------------------------------------------
     */

    if (options.verbose)
    {
        build_phase_timing(
            "linker",
            time_link,
            build_now()
        );
    }

    if (has_manifest_build)
    {
        build_status_finished(
            build_profile_name(build_profile),
            build_now() - build_start,
            0
        );

        /*
         * Guardar la caché de fuentes para poder saltarse
         * la próxima compilación si nada cambia.
         */
        build_cache_write(
            build_paths,
            build_manifest_path,
            options.input_file,
            module_sources,
            module_source_count,
            options.optimize
        );
    }
    else
    {
        printf(
            "Ejecutable generado: %s\n",
            options.output_file
        );
    }


    /*
     * -----------------------------------------------------
     * EJECUCIÓN (--run)
     * -----------------------------------------------------
     */

    int exit_code = 0;

    if (options.run_after)
        exit_code =
            run_compiled_binary(&options);


    /*
     * -----------------------------------------------------
     * LIMPIEZA
     * -----------------------------------------------------
     */

    linker_destroy(linker);

    llvm_destroy(llvm);
    llvm_shutdown();

    semantic_destroy(semantic);
    ast_destroy(program);

    error_context_destroy(errors);

    free(source);

    free(generated_output);

    /*
     * Borrar objeto temporal.
     *
     * Solo si no se pidió --keep-object. En proyectos con
     * manifiesto el objeto vive en target/ como cualquier
     * otro artefacto: no se borra.
     */

    if (!options.keep_object && !has_manifest_build)
    {
        remove(options.object_file);

        for (int i = 0;
             i < external_object_count;
             i++)
        {
            remove(external_objects[i]);
        }
    }

    for (int i = 0; i < external_object_count; i++)
        free(external_objects[i]);

    free(external_objects);

    for (int i = 0; i < external_source_count; i++)
        free(external_sources[i]);

    free(external_sources);
    free(program_dir);

    free(generated_object);

    for (int i = 0; i < module_source_count; i++)
        free(module_sources[i]);

    free(module_sources);

    free(options.run_args);

    free(manifest_entry);

    build_state_destroy(
        build_paths,
        build_project_dir,
        build_manifest_path,
        build_project_name,
        build_project_version,
        build_binary_name
    );

    return exit_code;
}