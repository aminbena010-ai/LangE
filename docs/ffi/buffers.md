# Buffers en FFI (`buffer<T>`)

`buffer<T>` permite pasar una `list<T>` E++ a una función
C como par puntero + longitud.

## Sintaxis

```rivo
external "lib.c" fn suma_buffer(buf:buffer<int>) -> int
```

En C, la función se firma con dos parámetros:

```c
int64_t suma_buffer(const int64_t *buf, int64_t n);
```

## Mecanismo

- `buffer<T>` solo puede ser **parámetro** de función
  `external` (no retorno, no variable local).
- El argumento E++ debe ser una `list<U>` donde `U` sea
  compatible con `T`.
- El compilador extrae el puntero al backing store y la
  longitud del `RivoList` del runtime.
- C recibe: puntero al primer elemento + número de
  elementos.

## Ejemplo E++

```rivo
external "buffer.c" fn suma_buffer(buf:buffer<int>) -> int

fn main():
    var nums:list<int> = [1, 2, 3, 4, 5]
    print(suma_buffer(nums))  # 15
```

## Ejemplo C

```c
#include <stdint.h>

int64_t suma_buffer(const int64_t *buf, int64_t n)
{
    int64_t total = 0;
    for (int64_t i = 0; i < n; i++)
        total += buf[i];
    return total;
}
```

## Errores

- `buffer<T>` como retorno:
  `una funcion externa no puede devolver un buffer`
- Argumento no compatible con `buffer<T>`:
  `incorrect argument type`

## Verificación ABI

Con `--ffi-check`, `buffer<T>` se expande en `(void *,
int64_t)` en el header generado.
