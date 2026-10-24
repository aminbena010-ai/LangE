# Tipos

Rivo comprueba estáticamente los tipos de expresiones, argumentos, retornos y
asignaciones. Anota el tipo tras `:`. No hay conversión implícita entre
`int` y `float`.

| Tipo | Ejemplo |
| --- | --- |
| `int` | `42` |
| `float` | `3.14` |
| `string` | `"hola"` |
| `bool` | `true`, `false` |
| `char` | `'x'` |
| `list<T>` | `list<int>` |
| `map<K, V>` | `map<string, int>` |
| `struct` | Un tipo compuesto declarado por el programa. |
| `task<T>` | `task<int>` |
| `null` | `null` |

`task<T>` es el tipo de una llamada a una `async fn`: el frontend lo acepta,
pero el codegen todavía no compila programas async, así que no lo uses en
programas que necesites compilar.

```rivo
fn main():
    var cantidad:int = 3
    var precio:float = 1.5
    print(cantidad)
    print(precio)
```

Usa `float` como nombre de tipo. Páginas: [int](int.md), [float](float.md),
[string](string.md), [bool](bool.md), [char](char.md), [list](list.md),
[map](map.md), [struct](struct.md) y [null](null.md).
