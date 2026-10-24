# Internals: generación LLVM

`src/llvm.c` traduce el AST ya validado a LLVM IR y declara las funciones
externas del runtime. El backend representa enteros, floats, punteros de texto
y handles de colecciones.

```sh
./build/rivo programa.ro -emit-ir
```

Inspecciona IR generado para depurar una representación; evita depender de
nombres internos temporales, que no forman parte de la sintaxis de Rivo.
