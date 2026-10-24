# `list<T>`

`list<T>` es una colección ordenada cuyos elementos tienen el mismo tipo.
Declara el tipo en la variable; el acceso empieza en el índice cero.

```rivo
fn main():
    var numeros:list<int> = [4, 8, 12]
    print(numeros[0])
    numeros.add(16)
```

Se puede iterar con `for`, concatenar listas compatibles y consultar longitud
con `len`. Los índices inválidos son errores de runtime. Véase
[operaciones de listas](../collections/lists.md).
