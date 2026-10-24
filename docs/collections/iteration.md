# Iterar colecciones

`for elemento in lista:` visita sus elementos. Los mapas también son
iterables; inspecciona el orden y el valor expuesto por la versión concreta
antes de depender de él.

```rivo
fn main():
    var nombres:list<string> = ["Ada", "Lin"]
    for nombre in nombres:
        print(nombre)
```

Para iterar una cantidad numérica puedes construir una lista con `range(inicio,
fin)`. Consulta [listas](lists.md) y [mapas](maps.md).
