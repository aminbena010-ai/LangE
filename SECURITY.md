# Seguridad

## Versiones soportadas

Rivo está en fase pre-1.0. Solo la última versión publicada recibe
correcciones de seguridad.

| Versión | Soporte |
| --- | --- |
| 0.8.x | Sí |
| < 0.8 | No |

## Reportar una vulnerabilidad

No abras un issue público para problemas de seguridad. Usa el canal privado
del repositorio (GitHub Security Advisories) o contacta directamente con los
mantenedores. Incluye:

- versión de Rivo (`rivo --version`) y plataforma;
- pasos mínimos para reproducirlo;
- impacto esperado (lectura/escritura de memoria, ejecución de código, etc.).

Se confirmará la recepción y se coordinará la divulgación.

## Firma de paquetes de release

Los paquetes de distribución se firman con una clave GPG **Ed25519** dedicada
a releases. La clave pública está en
[`keys/rivo-release-key.asc`](keys/rivo-release-key.asc).

Huella (fingerprint) de la clave de release:

```text
FF06 2D2E 5B00 D674 50B4  BCE1 A431 0A7E FD22 9C48
```

Verificación de un paquete:

```sh
gpg --import keys/rivo-release-key.asc
gpg --verify rivo-linux-x86_64-<version>.tar.gz.asc \
    rivo-linux-x86_64-<version>.tar.gz
sha256sum -c SHA256SUMS
```

Comprueba siempre el fingerprint por un canal independiente antes de confiar
en una clave.

## Alcance

Entran en el alcance: el compilador, el runtime, la biblioteca estándar, el
gestor de paquetes y los instaladores. No entran en el alcance los programas
de terceros ni las dependencias externas (LLVM, clang).
