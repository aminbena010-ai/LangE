# Changelog de Rivo

Todas las versiones relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es/1.0.0/) y versionado
semántico para las versiones estables (1.0+).

## [0.8.0+08a2] — 2026-10-24

Cierre de v0.8 «Optimización y adaptabilidad». Sin cambios de lenguaje.

### Añadido

- `make bench` (`tests/bench/bench.sh`): línea base de rendimiento de arranque
  del CLI, compilación en frío, caché y corpus.
- Caché de compilación por huella SHA-256 (`<binario>.huella`): invalida por
  contenido, nivel `-O`, versión del compilador y runtime; un `touch` ya no
  recompila.
- `SHA-256` interno (`src/hash.c`), sin dependencias externas.
- Matriz de soporte en `docs/development/support.md` y CI multiplataforma en
  `.github/workflows/ci.yml` (Linux x86_64/ARM64, macOS, Windows).
- `-O3` real: pipeline `default<O3>` en LLVM y `-O3` al enlazador.
- `-v` muestra también el tiempo de enlace (`[rivo] linker: X ms`).

### Corregido

- El nivel de optimización se truncaba a booleano al configurar el linker, que
  enlazaba siempre con `-O1`.
- El linker comprobaba `clang --version` en cada compilación (proceso extra);
  ahora busca el driver en `PATH` sin lanzarlo. Corpus de pruebas ~23% más
  rápido.
- Desbordamiento de un byte al construir la ruta de un módulo: `.ro` se
  copiaba con su NUL desde un literal de 4 bytes (detectado con
  `-Warray-bounds`).

### Documentación

- `docs/compiler/build.md`: caché por huella; `cli.md` y `options.md`: `-O3` y
  avisos como errores.

### Limitaciones conocidas

- Windows: `async`/`await` usa el respaldo ansioso (sin suspensión real);
  los tests de intercalado se omiten allí.

## [0.7.0+07a1] — 2026-10-23

- FFI profesional (fases 1–3): enteros de ancho fijo, `opaque`, strings con
  propiedad, callbacks, structs POD, `buffer<T>` y `--ffi-check`.
- 11 páginas de documentación en `docs/ffi/`.

## [0.6.0+06a1] — 2026-10-23

- Ecosistema remoto con índice Git estático: `[registro]`, `add`/`search`/
  `info`/`publish`, instalación desde Git.

## [0.5.0+05a1] — 2026-10-23

- Gestor de paquetes local: semver, `rivo.lock`, `add`/`install`/`update`/
  `remove`/`tree`/`list`.

## [0.4.0+04a1] — 2026-10-23

- `async`/`await` ejecutable con runtime de tareas (modelo ansioso).

## [0.3.0+03a1] — 2026-10-22

- Strings con comparaciones ordenadas y `len()` unificado.
