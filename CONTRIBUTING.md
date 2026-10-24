# Contribuir a Rivo

Gracias por querer ayudar. Este documento resume el flujo de trabajo.

## Antes de empezar

- Busca si ya existe un issue relacionado.
- Para cambios grandes (sintaxis, runtime, formato de paquetes), abre primero
  un issue para acordar el diseño.
- El proyecto sigue una prioridad estricta: **corrección, coherencia,
  seguridad de memoria, diagnósticos claros, mantenibilidad y rendimiento**.

## Entorno

Requisitos: CMake ≥ 3.16, compilador C11, LLVM con headers y `clang`.

```sh
cmake -S . -B build
cmake --build build
```

## Pruebas obligatorias

Ningún cambio se acepta si la suite no está en verde:

```sh
make test
make test-runtime
```

Si tu cambio afecta al rendimiento, ejecuta también `make bench` y adjunta
antes/después.

Si tocas código Rivo de `tests/` o `lib/`, pasa el formateador:

```sh
./build/rivo fmt tests lib
./build/rivo fmt --check tests lib
```

## Estilo de código

- C11, sin dependencias nuevas salvo justificación en el issue.
- Sigue el estilo del archivo que edites (nombres, indentación, comentarios).
- Los mensajes de error y la documentación van en español.
- Añade una prueba por cada cambio de comportamiento.

## Commits y pull requests

- Commits pequeños y con mensaje en imperativo (p. ej. «Añade caché por
  huella»).
- Un PR = un cambio coherente; describe el qué y el porqué.
- No se crean etiquetas (tags) en el repositorio: las versiones se registran en
  `CHANGELOG.md`.
- No incluyas artefactos generados (`build/`, `dist/`, `*.o`, `*.vsix`).

## Licencia

Al contribuir aceptas que tu aportación se publique bajo la licencia MIT del
proyecto.
