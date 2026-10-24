/* =========================================================
 * SEMVER (versionado semantico para paquetes)
 *
 * Version:      1.2.3   (1 a 3 componentes numericos)
 * Restricciones:
 *      "1.2.3"   -> ^1.2.3  (compatible por defecto)
 *      "^1.2.3"  -> >=1.2.3, <2.0.0
 *      "~1.2.3"  -> >=1.2.3, <1.3.0
 *      "=1.2.3"  -> exacta
 *      ">=1.2.3", ">1.2.3", "<=1.2.3", "<1.2.3"
 *      "*"       -> cualquiera
 *      ">=1.0, <2.0" -> varias restricciones (AND)
 * ======================================================= */

#ifndef RIVO_SEMVER_H
#define RIVO_SEMVER_H

typedef struct SemVer {
    int major;
    int minor;
    int patch;

    /* Componentes escritos: 1, 2 o 3 (para ^ y ~) */
    int parts;
} SemVer;

/* Devuelve 1 si `text` es una version valida. */
int semver_parse(const char *text, SemVer *out);

/* -1, 0 o 1 comparando versiones. */
int semver_compare(const SemVer *left, const SemVer *right);

/* 1 si `requirement` es una restriccion sintacticamente valida. */
int semver_valid_requirement(const char *requirement);

/*
 * 1 si `version` cumple la restriccion, 0 si no,
 * -1 si la restriccion no es valida.
 */
int semver_satisfies(const char *requirement, const SemVer *version);

#endif /* RIVO_SEMVER_H */
