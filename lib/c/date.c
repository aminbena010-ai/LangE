/*
 * Biblioteca estandar de E++: fecha.
 *
 * La declara lib/date.rivo con `external`. El calculo de
 * calendario es aritmetica pura (dias civiles desde
 * 1970-01-01), asi que las conversiones epoca <-> fecha no
 * dependen de la zona horaria (funcionan en UTC). Solo
 * current_date, current_time y current_datetime usan la
 * hora local del sistema.
 *
 * Los textos salen de un anillo de buffers estaticos: el
 * runtime no copia lo que devuelve C (claude.md §49), y con
 * varios buffers una llamada no pisa a la anterior dentro
 * de la misma expresion.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>


#define BUFFER_COUNT 8
#define BUFFER_SIZE 32


static char buffers[BUFFER_COUNT][BUFFER_SIZE];
static int buffer_siguiente_indice = 0;


static char *next_buffer(void)
{
    char *buffer = buffers[buffer_siguiente_indice];

    buffer_siguiente_indice =
        (buffer_siguiente_indice + 1) % BUFFER_COUNT;

    return buffer;
}


static int is_leap(long long anio)
{
    return (anio % 4 == 0 && anio % 100 != 0) ||
           (anio % 400 == 0);
}


static int days_of_month(long long anio, int mes)
{
    static const int dias[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (mes < 1 || mes > 12)
        return 0;

    if (mes == 2 && is_leap(anio))
        return 29;

    return dias[mes - 1];
}


/*
 * Dias desde 1970-01-01 hasta (anio, mes, dia) — algoritmo
 * de dias civiles de Howard Hinnant, sin timegm ni zoneinfo.
 */
static long long days_from_civil(
    long long anio,
    int mes,
    int dia
)
{
    long long ajuste = anio - (mes <= 2 ? 1 : 0);
    long long era =
        (ajuste >= 0 ? ajuste : ajuste - 399) / 400;
    long long anyo_era = ajuste - era * 400;
    long long dia_anyo =
        (153 * (mes + (mes > 2 ? -3 : 9)) + 2) / 5 +
        dia - 1;
    long long dia_era =
        anyo_era * 365 +
        anyo_era / 4 -
        anyo_era / 100 +
        dia_anyo;

    return era * 146097 + dia_era - 719468;
}


static void days_to_civil(
    long long dias,
    long long *anio,
    int *mes,
    int *dia
)
{
    long long z = dias + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long dia_era = z - era * 146097;
    long long anyo_era =
        (dia_era - dia_era / 1460 +
         dia_era / 36524 -
         dia_era / 146096) / 365;
    long long anio_base = anyo_era + era * 400;
    long long dia_anyo =
        dia_era -
        (365 * anyo_era + anyo_era / 4 - anyo_era / 100);
    long long mes_base = (5 * dia_anyo + 2) / 153;
    long long dia_mes =
        dia_anyo - (153 * mes_base + 2) / 5 + 1;
    long long mes_final =
        mes_base + (mes_base < 10 ? 3 : -9);

    *anio = anio_base + (mes_final <= 2 ? 1 : 0);
    *mes = (int) mes_final;
    *dia = (int) dia_mes;
}


/*
 * "AAAA-MM-DD" exacto con el mes y el dia validos para el
 * anyo. Devuelve 1 si es valida.
 */
static int parse_date(
    const char *fecha,
    long long *anio,
    int *mes,
    int *dia
)
{
    if (fecha == NULL)
        return 0;

    int largo = 0;

    while (fecha[largo] != '\0') {

        if (largo >= 10)
            return 0;

        largo++;
    }

    if (largo != 10)
        return 0;

    if (fecha[4] != '-' || fecha[7] != '-')
        return 0;

    for (int i = 0; i < 10; i++) {

        if (i == 4 || i == 7)
            continue;

        if (fecha[i] < '0' || fecha[i] > '9')
            return 0;
    }

    *anio =
        (long long)(fecha[0] - '0') * 1000 +
        (long long)(fecha[1] - '0') * 100 +
        (long long)(fecha[2] - '0') * 10 +
        (long long)(fecha[3] - '0');

    *mes =
        (fecha[5] - '0') * 10 +
        (fecha[6] - '0');

    *dia =
        (fecha[8] - '0') * 10 +
        (fecha[9] - '0');

    if (*mes < 1 || *mes > 12)
        return 0;

    if (*dia < 1 ||
        *dia > days_of_month(*anio, *mes))
        return 0;

    return 1;
}


static const char *format_date(
    long long anio,
    int mes,
    int dia
)
{
    char *buffer = next_buffer();

    snprintf(
        buffer,
        BUFFER_SIZE,
        "%04lld-%02d-%02d",
        anio,
        mes,
        dia
    );

    return buffer;
}


const char *current_date(void)
{
    char *buffer = next_buffer();
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);

    if (local == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strftime(buffer, BUFFER_SIZE, "%Y-%m-%d", local);

    return buffer;
}


const char *current_time(void)
{
    char *buffer = next_buffer();
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);

    if (local == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strftime(buffer, BUFFER_SIZE, "%H:%M:%S", local);

    return buffer;
}


const char *current_datetime(void)
{
    char *buffer = next_buffer();
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);

    if (local == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strftime(
        buffer,
        BUFFER_SIZE,
        "%Y-%m-%d %H:%M:%S",
        local
    );

    return buffer;
}


const char *epoch_to_date(long long segundos)
{
    char *buffer = next_buffer();
    time_t momento = (time_t) segundos;
    struct tm *utc = gmtime(&momento);

    if (utc == NULL) {
        buffer[0] = '\0';
        return buffer;
    }

    strftime(buffer, BUFFER_SIZE, "%Y-%m-%d", utc);

    return buffer;
}


long long date_to_epoch(const char *fecha)
{
    long long anio = 0;
    int mes = 0;
    int dia = 0;

    if (!parse_date(fecha, &anio, &mes, &dia))
        return -1;

    return days_from_civil(anio, mes, dia) * 86400;
}


const char *add_days(const char *fecha, long long dias)
{
    long long anio = 0;
    int mes = 0;
    int dia = 0;

    if (!parse_date(fecha, &anio, &mes, &dia))
        return "";

    long long base = days_from_civil(anio, mes, dia);
    long long resultado_anio = 0;
    int resultado_mes = 0;
    int resultado_dia = 0;

    days_to_civil(
        base + dias,
        &resultado_anio,
        &resultado_mes,
        &resultado_dia
    );

    return format_date(
        resultado_anio,
        resultado_mes,
        resultado_dia
    );
}


/*
 * days_between(inicio, fin) = fin - inicio en dias; 0 si
 * alguna de las dos fechas no es valida.
 */
long long days_between(const char *inicio, const char *fin)
{
    long long anio_inicio = 0;
    int mes_inicio = 0;
    int dia_inicio = 0;
    long long anio_fin = 0;
    int mes_fin = 0;
    int dia_fin = 0;

    if (!parse_date(
            inicio,
            &anio_inicio,
            &mes_inicio,
            &dia_inicio))
        return 0;

    if (!parse_date(
            fin,
            &anio_fin,
            &mes_fin,
            &dia_fin))
        return 0;

    return days_from_civil(anio_fin, mes_fin, dia_fin) -
           days_from_civil(
               anio_inicio,
               mes_inicio,
               dia_inicio);
}


long long is_leap_year(long long anio)
{
    return is_leap(anio) ? 1 : 0;
}


unsigned char is_valid_date(const char *fecha)
{
    long long anio = 0;
    int mes = 0;
    int dia = 0;

    return parse_date(fecha, &anio, &mes, &dia) ? 1 : 0;
}


/*
 * 0 = domingo ... 6 = sabado; -1 si la fecha no es valida.
 */
long long day_of_week(const char *fecha)
{
    long long anio = 0;
    int mes = 0;
    int dia = 0;

    if (!parse_date(fecha, &anio, &mes, &dia))
        return -1;

    /*
     * 1970-01-01 (dia 0) fue jueves: con 0 = domingo,
     * jueves cae en el indice 4.
     */
    long long indice =
        (days_from_civil(anio, mes, dia) + 4) % 7;

    if (indice < 0)
        indice += 7;

    return indice;
}


const char *day_of_week_name(const char *fecha)
{
    static const char *nombres[7] = {
        "domingo",
        "lunes",
        "martes",
        "miercoles",
        "jueves",
        "viernes",
        "sabado"
    };

    long long indice = day_of_week(fecha);

    if (indice < 0)
        return "";

    return nombres[indice];
}


long long days_in_month(long long anio, long long mes)
{
    if (mes < 1 || mes > 12)
        return 0;

    return days_of_month(anio, (int) mes);
}


const char *month_name(long long mes)
{
    static const char *nombres[12] = {
        "enero",
        "febrero",
        "marzo",
        "abril",
        "mayo",
        "junio",
        "julio",
        "agosto",
        "septiembre",
        "octubre",
        "noviembre",
        "diciembre"
    };

    if (mes < 1 || mes > 12)
        return "";

    return nombres[mes - 1];
}
