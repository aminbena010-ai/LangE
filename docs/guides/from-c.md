# De C a Rivo

Rivo gestiona strings y colecciones mediante su runtime; no declara bloques
con llaves ni usa `;`. Los tipos de parámetros y variables son explícitos.

```rivo
fn sumar(a:int, b:int) -> int:
    return a + b

fn main():
    var resultado:int = sumar(2, 3)
    print(resultado)
```

Para reutilizar una biblioteca C, declara funciones mediante `external`.
Consulta [FFI](../ffi/external.md) y [tipos permitidos](../ffi/allowed-types.md).
