# De Python a Rivo

Rivo comparte la indentación por bloques, pero requiere anotaciones de tipo y
usa palabras clave en inglés. Sí usa `elif` igual que Python; no hay
parámetros por defecto ni conversión implícita.

| Python | Rivo |
| --- | --- |
| `def doble(n):` | `fn doble(n:int) -> int:` |
| `print(x)` | `print(x)` |
| `None` | `null` |
| `True` / `False` | `true` / `false` |
| `for x in xs:` | `for x in xs:` |

```rivo
fn doble(n:int) -> int:
    return n * 2

fn main():
    print(doble(5))
```

Compila cada ejemplo con `rivo archivo.ro`; consulta la [referencia de tipos](../types/overview.md).
