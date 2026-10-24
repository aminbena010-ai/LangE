# Internals: AST

`include/ast.h` define los nodos con los que el parser representa declaraciones,
sentencias y expresiones. La fase semántica y el generador LLVM consumen ese
árbol.

```text
AST → análisis semántico → LLVM IR
```

Un nuevo nodo suele requerir actualización de creación/destrucción, análisis,
codegen y pruebas. El AST no es una API pública del lenguaje.
