/* =========================================================
 * SHA-256 (para la caché de compilación)
 *
 * Implementación interna y portable: sin OpenSSL ni
 * procesos externos, para no añadir dependencias en
 * Linux/macOS/Windows.
 * ========================================================= */

#ifndef RIVO_HASH_H
#define RIVO_HASH_H

#include <stddef.h>

/*
 * Calcula el SHA-256 de un bloque de memoria.
 * out debe apuntar a 32 bytes.
 */
void hash_sha256(
    const void *data,
    size_t length,
    unsigned char out[32]
);

/*
 * SHA-256 del contenido de un archivo.
 * 1 = ok, 0 = no se pudo leer.
 */
int hash_file_sha256(
    const char *path,
    unsigned char out[32]
);

/* Hex minúsculas de 32 bytes: out debe tener 65 bytes. */
void hash_hex(
    const unsigned char digest[32],
    char out[65]
);

#endif
