# Arquitectura del compilador

El compilador está organizado en C:

| Componente | Archivos principales |
| --- | --- |
| Tokens y lexer | `include/token.h`, `src/token.c`, `src/lexer.c` |
| Parser y AST | `include/parser.h`, `src/parser.c`, `src/ast.c` |
| Semántica | `include/semantic.h`, `src/semantic.c` |
| LLVM | `include/llvm.h`, `src/llvm.c` |
| Enlace/CLI | `src/linker.c`, `src/main.c` |
| Manifiesto `rivo.toml` | `include/manifest.h`, `src/manifest.c` |
| Build `target/`, caché y estado | `include/build.h`, `src/build.c` |
| Errores y sugerencias | `src/error.c`, `src/suggest.c` |
| Runtime | `src/runtime/` |
| Biblioteca | `lib/*.ro`, `lib/c/` |

```sh
cmake --build build
```

Esta división describe la implementación; no garantiza que cada estructura
interna sea una API estable. Ver [pipeline](pipeline.md) e
[internals](../internals/lexer.md).
