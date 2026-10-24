# Pipeline de compilación

El compilador procesa un fuente en varias fases:

```text
[manifiesto rivo.toml + caché] → archivo .ro → lexer → parser/AST
           → análisis semántico → LLVM IR → objeto → clang + runtime
           → ejecutable en target/<perfil>/
```

```sh
./build/rivo ejemplo.ro -emit-ir
```

Si hay un manifiesto [`rivo.toml`](manifest.md), antes de leer la fuente se
comprueba la caché de [`target/`](build.md): si ninguna fuentes cambió, se
reutiliza el binario y el pipeline termina ahí. Un error de sintaxis o tipos
detiene las fases posteriores. Los módulos importados se incorporan al
análisis antes de generar código. La salida LLVM
permite inspeccionar la representación intermedia.
