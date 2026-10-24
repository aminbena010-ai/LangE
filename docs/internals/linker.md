# Internals: enlazador

`src/linker.c` coordina la salida objeto/ensamblador y el enlace del programa
con `clang` y `librivo_runtime`. Las funciones `external` agregan los objetos C
requeridos.

```sh
./build/rivo programa.ro -c
```

La ubicación del runtime y de la biblioteca estándar se resuelve en tiempo de
compilación o en relación con el ejecutable instalado. Consulta
[opciones CLI](../compiler/options.md).
