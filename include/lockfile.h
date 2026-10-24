/* =========================================================
 * LOCKFILE (rivo.lock)
 *
 * Fija la version exacta y el origen de cada dependencia
 * resuelta. Se commitea al repositorio y solo se regenera
 * con `rivo update`.
 * ======================================================= */

#ifndef RIVO_LOCKFILE_H
#define RIVO_LOCKFILE_H

#define RIVO_LOCK_FILENAME "rivo.lock"

typedef struct LockedPackage {

    char *name;
    char *version;

    /* "ruta:../libs/x" o "registro:https://..." */
    char *source;

    /* Nombres de sus dependencias directas */
    char **deps;
    int dep_count;

} LockedPackage;

typedef struct LockFile {

    LockedPackage *packages;
    int count;

} LockFile;


LockFile *lockfile_new(void);

void lockfile_destroy(LockFile *lock);

LockedPackage *lockfile_find(
    LockFile *lock,
    const char *name
);

int lockfile_add(
    LockFile *lock,
    const char *name,
    const char *version,
    const char *source,
    const char *const *deps,
    int dep_count
);

/* Escribe el lockfile. 1 = ok. */
int lockfile_save(
    const LockFile *lock,
    const char *path
);

/* Carga el lockfile. NULL si no existe o no es valido. */
LockFile *lockfile_load(const char *path);

#endif /* RIVO_LOCKFILE_H */
