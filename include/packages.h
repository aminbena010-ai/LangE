/* =========================================================
 * GESTOR DE PAQUETES (v0.5, solo local)
 *
 * Subcomandos: install, update, add, remove, tree, list.
 * Las dependencias de version se buscan en .rivo/deps/<nombre>
 * o vendor/<nombre>; el registro remoto llega en la v0.6.
 * ======================================================= */

#ifndef RIVO_PACKAGES_H
#define RIVO_PACKAGES_H

/* 1 si `name` es un subcomando del gestor. */
int packages_is_command(const char *name);

/*
 * Ejecuta un subcomando. argv[0] es el nombre del
 * subcomando. Devuelve el codigo de salida del proceso.
 */
int packages_run(int argc, char **argv);

#endif /* RIVO_PACKAGES_H */
