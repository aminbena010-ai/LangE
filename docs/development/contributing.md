# Contribuir

Antes de modificar el compilador, localiza la fase responsable y una prueba
existente similar. Mantén coherentes lexer, parser, semántica, codegen y
runtime cuando agregues sintaxis o funciones.

```sh
cmake --build build
sh tests/run_tests.sh
sh tests/runtime/run_tests.sh
```

Incluye pruebas para el caso normal y errores relevantes; ejecuta las suites
pertinentes y comunica cualquier fallo preexistente de forma separada. Consulta
el [mapa de componentes](project-structure.md).
