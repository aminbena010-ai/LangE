# Structs POD en FFI

Los structs E++ cuyos campos son todos escalares FFI se
pueden pasar a funciones `external` por referencia
(puntero `void *`).

## Reglas

- Todos los campos deben ser escalares FFI: `int`, `float`,
  `bool`, `char`, `i8`..`u64`, `size`.
- **Prohibido**: `string`, structs anidados, `list`, `map`
  como campos.
- El struct se pasa como puntero a C (`void *`).
- El usuario C castea a su propio tipo con el mismo layout.

## Ejemplo E++

```rivo
struct Punto:
    var x:int
    var y:int

external "punto.c" fn distancia(p:Punto) -> float

fn main():
    var p:Punto = Punto(3, 4)
    print(distancia(p))
```

## Ejemplo C

```c
#include <stdint.h>
#include <math.h>

typedef struct {
    int64_t x;
    int64_t y;
} RivoPunto;

double distancia(const void *p)
{
    const RivoPunto *punto = (const RivoPunto *) p;
    double dx = (double) punto->x;
    double dy = (double) punto->y;
    return sqrt(dx * dx + dy * dy);
}
```

## Errores

Si un campo no es escalares FFI:

```
error: los parametros de una funcion externa deben ser
int, float, string, bool, char, opaque, callback, i8, ...
o un struct POD
```

## Verificación ABI

Con `--ffi-check`, los structs se mapean a `void *` en el
header generado. El layout real lo verifica el usuario C
al definir su propio typedef.
