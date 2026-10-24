# Bucle `for`

`for nombre in iterable:` recorre una lista o un mapa. En listas, cada
iteración entrega un elemento; en mapas, el comportamiento de recorrido está
definido por la implementación y conviene probarlo con la versión objetivo.

```rivo
fn main():
    var valores:list<int> = [2, 4, 6]
    for valor in valores:
        print(valor)
```

El tipo de la variable de iteración se deduce del iterable; no se declara con
`var` en el encabezado.
