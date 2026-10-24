# F-strings

Un f-string comienza con `f"` e interpola expresiones entre `{}`. Se admiten
valores escalares (`int`, `float`, `string`, `bool`, `char`), llamadas e
índices; no se admiten operadores dentro de la interpolación.

```rivo
fn doble(n:int) -> int:
    return n * 2

fn main():
    var n:int = 4
    print(f"n={n}, doble={doble(n)}")
    print(f"llaves: {{texto}}")
```

`{{` y `}}` producen llaves literales. Listas, mapas y `null` no se pueden
interpolar directamente; llama a una función de conversión apropiada.
