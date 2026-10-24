/*
 * Biblioteca estandar de E++: sistema.
 *
 * La declara lib/system.rivo con `external`. Consultas al
 * sistema operativo: identidad, entorno y recursos.
 *
 * Los textos que se generan usan un anillo de buffers
 * estaticos (claude.md §49: el runtime no copia lo que
 * devuelve C). get_env() y username() devuelven
 * memoria estable de libc o "", no el anillo.
 *
 * Las métricas de memoria y uptime se implementan en Linux
 * y Windows; en macOS devuelven -1.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#endif


#define BUFFER_COUNT 8
#define BUFFER_SIZE 4096


static char buffers[BUFFER_COUNT][BUFFER_SIZE];
static int buffer_siguiente_indice = 0;


static char *next_buffer(void)
{
    char *buffer = buffers[buffer_siguiente_indice];

    buffer_siguiente_indice =
        (buffer_siguiente_indice + 1) % BUFFER_COUNT;

    return buffer;
}


long long pid(void)
{
#ifdef _WIN32
    return (long long) GetCurrentProcessId();
#else
    return (long long) getpid();
#endif
}


const char *current_directory(void)
{
    char *buffer = next_buffer();

#ifdef _WIN32
    if (_getcwd(buffer, BUFFER_SIZE) == NULL)
#else
    if (getcwd(buffer, BUFFER_SIZE) == NULL)
#endif
        buffer[0] = '\0';

    return buffer;
}


static const char *uname_field(int cual)
{
    char *buffer = next_buffer();
#ifdef _WIN32
    if (cual == 0) {
        DWORD length = BUFFER_SIZE;
        if (!GetComputerNameA(buffer, &length))
            buffer[0] = '\0';
        return buffer;
    }

    if (cual == 1) {
        snprintf(buffer, BUFFER_SIZE, "%s", "Windows");
        return buffer;
    }

#if defined(_M_ARM64) || defined(__aarch64__)
    snprintf(buffer, BUFFER_SIZE, "%s", "ARM64");
#elif defined(_M_X64) || defined(__x86_64__)
    snprintf(buffer, BUFFER_SIZE, "%s", "x86_64");
#else
    snprintf(buffer, BUFFER_SIZE, "%s", "x86");
#endif
    return buffer;
#else
    struct utsname info;

    if (uname(&info) != 0) {
        buffer[0] = '\0';
        return buffer;
    }

    const char *valor = "";

    if (cual == 0)
        valor = info.nodename;
    else if (cual == 1)
        valor = info.sysname;
    else
        valor = info.machine;

    snprintf(buffer, BUFFER_SIZE, "%s", valor);

    return buffer;
#endif
}


const char *hostname(void)
{
    return uname_field(0);
}


const char *os(void)
{
    return uname_field(1);
}


const char *architecture(void)
{
    return uname_field(2);
}


/*
 * El bloque que devuelve getenv() vive hasta que cambie el
 * entorno: no se copia (asi no se trunca ningun valor).
 */
const char *get_env(const char *nombre)
{
    if (nombre == NULL)
        return "";

    const char *valor = getenv(nombre);

    return valor == NULL ? "" : valor;
}


unsigned char has_env(const char *nombre)
{
    if (nombre == NULL)
        return 0;

    return getenv(nombre) != NULL ? 1 : 0;
}


unsigned char set_env(
    const char *nombre,
    const char *valor
)
{
    if (nombre == NULL ||
        valor == NULL ||
        nombre[0] == '\0')
        return 0;

#ifdef _WIN32
    return _putenv_s(nombre, valor) == 0 ? 1 : 0;
#else
    return setenv(nombre, valor, 1) == 0 ? 1 : 0;
#endif
}


const char *username(void)
{
#ifdef _WIN32
    char *buffer = next_buffer();
    DWORD length = BUFFER_SIZE;
    if (!GetEnvironmentVariableA("USERNAME", buffer, length))
        buffer[0] = '\0';
    return buffer;
#else
    const char *valor = getenv("USER");

    if (valor == NULL) {

        struct passwd *cuenta = getpwuid(getuid());

        if (cuenta == NULL ||
            cuenta->pw_name == NULL)
            return "";

        valor = cuenta->pw_name;
    }

    char *buffer = next_buffer();

    snprintf(buffer, BUFFER_SIZE, "%s", valor);

    return buffer;
#endif
}


long long cpu_count(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0
        ? (long long) info.dwNumberOfProcessors
        : -1;
#else
    long cantidad = sysconf(_SC_NPROCESSORS_ONLN);

    if (cantidad < 1)
        return -1;

    return (long long) cantidad;
#endif
}


long long total_memory_mb(void)
{
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status)
        ? (long long) (status.ullTotalPhys / (1024ULL * 1024ULL))
        : -1;
#elif defined(__linux__)
    struct sysinfo info;

    if (sysinfo(&info) != 0)
        return -1;

    unsigned long long total =
        (unsigned long long) info.totalram *
        (unsigned long long) info.mem_unit;

    return (long long) (total / (1024ULL * 1024ULL));
#else
    return -1;
#endif
}


long long available_memory_mb(void)
{
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status)
        ? (long long) (status.ullAvailPhys / (1024ULL * 1024ULL))
        : -1;
#elif defined(__linux__)
    struct sysinfo info;

    if (sysinfo(&info) != 0)
        return -1;

    unsigned long long libre =
        (unsigned long long) info.freeram *
        (unsigned long long) info.mem_unit;

    return (long long) (libre / (1024ULL * 1024ULL));
#else
    return -1;
#endif
}


long long uptime(void)
{
#ifdef _WIN32
    return (long long) (GetTickCount64() / 1000ULL);
#elif defined(__linux__)
    struct sysinfo info;

    if (sysinfo(&info) != 0)
        return -1;

    return (long long) info.uptime;
#else
    return -1;
#endif
}
