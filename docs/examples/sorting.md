# Ejemplo: ordenar enteros

`sorted` devuelve una copia ordenada ascendentemente de `list<int>`:

```rivo
fn main():
    var datos:list<int> = [8, 3, 5, 1]
    var ordenados:list<int> = sorted(datos)
    var i:int = 0
    while i < len(ordenados):
        print(ordenados[i])
        i += 1
```

La lista original permanece sin cambios.
