# Listas

Una lista es ordenada y homogénea. Declárala con `list<T>` y un literal entre
corchetes; el índice inicial es cero.

```rivo
fn main():
    var xs:list<int> = [10, 20]
    xs.add(30)
    xs[1] = 25
    print(xs[1])
    print(len(xs))
```

Se puede indexar, asignar a índices existentes, añadir elementos, iterar y
concatenar listas del mismo tipo. Los índices fuera de rango producen error de
runtime. Véase [iteración](iteration.md) y [concatenación](concatenation.md).
