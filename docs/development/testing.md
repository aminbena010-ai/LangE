# Pruebas

Ejecuta las suites desde la raíz del repositorio:

```sh
sh tests/run_tests.sh
sh tests/runtime/run_tests.sh
```

La suite principal (`tests/run_tests.sh`) compila programas Rivo y compara
ejecuciones y diagnósticos; usa casos de `tests/integration/` y
`tests/stdlib/` (positivos), `tests/errors/` (negativos) y
`tests/fixtures/` (incluye proyectos con `rivo.toml`), y también ejemplos de
`tests/basic/`. La suite del runtime (`tests/runtime/run_tests.sh`) prueba el
código C del runtime. Añade casos positivos a `tests/integration/` o
`tests/stdlib/`, negativos a `tests/errors/` y registra cada prueba en el
runner correspondiente. Documenta la salida real de la suite: no conviertas
fallos conocidos en resultados de éxito.
