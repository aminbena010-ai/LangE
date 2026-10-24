# Handles opacos (opaque)

El tipo `opaque` representa un puntero opaco de C (`void *`)
sin exponer punteros crudos al usuario E++.

## Sintaxis

```rivo
external "lib.c" fn fopen(path:string, mode:string) -> opaque
external "lib.c" fn fclose(handle:opaque) -> int
```

`opaque` se puede usar en:

- Parámetros y retorno de funciones `external`.
- Variables locales (`var f:opaque = ...`).
- Campos de struct.

## Semántica

- `opaque` es nullable: se puede comparar con `null`.
- No se puede derreferenciar ni acceder a sus campos.
- Solo se puede pasar como argumento a funciones externas.
- No es un tipo FFI-only: se puede declarar en variables.

## Comparación

```rivo
var f:opaque = fopen("/dev/null", "w")

if f == null:
    print("error al abrir")
else:
    fclose(f)
```

La comparación usa identidad de puntero (`icmp`), no
comparación de contenido de cadena.

## Ejemplo: fopen/fclose

```rivo
external "../fixtures/opaque.c" fn abrir_archivo(
    ruta:string,
    modo:string
) -> opaque

external "../fixtures/opaque.c" fn cerrar_archivo(
    recurso:opaque
) -> int

fn main():
    var f:opaque = abrir_archivo("/dev/null", "w")
    print(cerrar_archivo(f))
```
