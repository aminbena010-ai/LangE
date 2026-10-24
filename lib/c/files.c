/*
 * Biblioteca estandar de E++: ficheros.
 *
 * La declara lib/files.rivo con `external`. Aquí solo
 * están las operaciones que las funciones incorporadas no
 * cubren (stat, borrar, crear directorio y renombrar);
 * leer y escribir se quedan en E++ para que los textos
 * vivan en la arena del runtime.
 *
 * Los directorios también cuentan como existentes: el
 * builtin file_exists usa fopen y no los puede abrir.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

long long file_size(const char *ruta)
{
#ifdef _WIN32
    struct _stat info;
    if (ruta == NULL || _stat(ruta, &info) != 0)
#else
    struct stat info;
    if (ruta == NULL || stat(ruta, &info) != 0)
#endif
        return -1;

    return (long long) info.st_size;
}

unsigned char file_exists(const char *ruta)
{
#ifdef _WIN32
    struct _stat info;
#else
    struct stat info;
#endif

    if (ruta == NULL)
        return 0;

#ifdef _WIN32
    return _stat(ruta, &info) == 0 ? 1 : 0;
#else
    return stat(ruta, &info) == 0 ? 1 : 0;
#endif
}

unsigned char delete_file(const char *ruta)
{
    if (ruta == NULL)
        return 0;

    return remove(ruta) == 0 ? 1 : 0;
}

unsigned char create_directory(const char *ruta)
{
    if (ruta == NULL)
        return 0;

    /*
     * Devuelve false si el directorio ya existía: no hay
     * forma de distinguirlo desde E++ sin errno.
     */
#ifdef _WIN32
    return _mkdir(ruta) == 0 ? 1 : 0;
#else
    return mkdir(ruta, 0755) == 0 ? 1 : 0;
#endif
}

unsigned char file_rename(const char *origen, const char *destino)
{
    if (origen == NULL || destino == NULL)
        return 0;

    return rename(origen, destino) == 0 ? 1 : 0;
}
