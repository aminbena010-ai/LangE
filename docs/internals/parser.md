# Internals: parser

`src/parser.c` consume tokens y crea nodos AST. Las expresiones se analizan
por niveles de precedencia: asignación, lógica, comparación, aritmética,
unarios y primarios.

```text
tokens → declaraciones/sentencias → AST_PROGRAM
```

Al modificar gramática, actualiza `include/parser.h`, el AST si hace falta y
las pruebas que cubren tanto entradas válidas como inválidas.
