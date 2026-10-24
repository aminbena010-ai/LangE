# Opciones del compilador

| Opción | Propósito |
| --- | --- |
| `-o archivo` | Nombre del ejecutable de salida. |
| `-I dir` o `-Idir` | Añade directorio de búsqueda de módulos/cabeceras. |
| `-l lib` o `-lib` | Biblioteca del sistema para el enlace final (p. ej. `-l m` enlaza `libm`). |
| `-L dir` o `-Ldir` | Directorio extra donde buscar esas bibliotecas. |
| `-c` | Emite objeto sin enlazar. |
| `-S` | Emite ensamblador. |
| `-emit-ir` | Emite LLVM IR. |
| `-g` | Incluye información de depuración. |
| `-O0`, `-O1`, `-O2`, `-O3` | Nivel de optimización (LLVM y linker). |
| `-v`, `--verbose` | Muestra información adicional y tiempos por fase. |
| `--keep-object` | Conserva el objeto intermedio. |
| `--run` | Ejecuta el binario tras compilar (como `cargo run`). |
| `--` | Separa los argumentos para el programa (con `--run`). |

```sh
./build/rivo programa.ro -g -O1 -o programa
./build/rivo programa.ro -emit-ir
./build/rivo programa.ro -L ./libs -l m
```

`-l` y `-L` alimentan el enlace final con clang, como en C; para declarar
funciones C en E++ usa `external` (véase [compilación y enlace](../ffi/build-and-link.md)).

Consulta la ayuda del binario de la versión instalada para flags vigentes.
