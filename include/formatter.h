#ifndef RIVO_FORMATTER_H
#define RIVO_FORMATTER_H

/*
 * Formateador conservador de Rivo.
 *
 * No reordena ni reescribe la estructura del programa: solo
 * normaliza espacios en blanco (finales de línea, tabuladores
 * de indentación, líneas vacías sobrantes y salto final).
 * Así nunca cambia el significado de un fichero válido.
 */

int formatter_run(
    int argc,
    char **argv
);

#endif
