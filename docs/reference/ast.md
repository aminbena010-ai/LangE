# AST

El parser convierte tokens a un árbol de sintaxis abstracta (AST). Hay nodos
para programa, funciones, declaraciones, sentencias de control, llamadas,
literales, colecciones e importaciones. La definición interna está en
`include/ast.h`.

```text
fuente → tokens → AST → análisis semántico
```

Los nodos y sus campos son detalles internos del compilador, no una interfaz
estable de plugins. Consulta [arquitectura](../compiler/architecture.md).
