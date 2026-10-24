# Funciones de colecciones

`len(valor)` devuelve la longitud de string, lista o mapa. Otras funciones
incorporadas son `range`, `divmod`, `sorted`, `reversed`, `all` y `any`.

```rivo
fn main():
    var xs:list<int> = range(1, 4)
    var ordenada:list<int> = sorted(xs)
    print(len(ordenada))
    print(ordenada[2])
```

`range(a, b)` produce enteros desde `a` hasta antes de `b`;
`divmod(a, b)` devuelve una lista de cociente y resto; `sorted` se limita a
`list<int>`; `reversed` devuelve una copia invertida. `all` y `any` reciben
`list<bool>`. Para mutación, las listas tienen métodos `add` y `remove`.
