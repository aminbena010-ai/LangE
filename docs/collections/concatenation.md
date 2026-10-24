# Concatenación de colecciones

`+` concatena dos listas si sus tipos de elemento son iguales. Devuelve una
nueva lista; `+=` vuelve a asignar el resultado a la variable izquierda.

```rivo
fn main():
    var a:list<int> = [1, 2]
    var b:list<int> = [3]
    var c:list<int> = a + b
    print(c[2])
```

No hay concatenación de mapas. Para strings, `+` concatena texto; véase
[concatenación de strings](../strings/concatenation.md).
