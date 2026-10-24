# Funciones variadic C (`...`)

Permite declarar funciones C variadic como `printf`.

## Sintaxis

```rivo
external "lib.c" fn log_externo(
    fmt:string, a:int, b:int, ...
) -> int
```

## Mecanismo

- `...` al final de los parámetros marca la función
  como variadic.
- Solo permitido en `external` (no en `fn` normal).
- Los parámetros fijos se comproban con la firma.
- Los argumentos extras no se comprueban (pasan tal cual).

## Ejemplo E++

```rivo
external "log.c" fn log_externo(
    fmt:string, nivel:int, ...
) -> int

fn main():
    log_externo("error %d\n", 3)
```

## Ejemplo C

```c
#include <stdio.h>
#include <stdint.h>

int64_t log_externo(const char *fmt, int64_t nivel, ...)
{
    return printf(fmt, (int)nivel);
}
```

## Errores

- `...` en `fn` normal: no permitido.
- Argumentos fijos con tipo incompatible:
  `incorrect argument type`.
- Menos argumentos que fijos:
  `faltan argumentos en la llamada variadic`.
