# Runtime de colecciones

Las listas y mapas Rivo están respaldados por estructuras del runtime. El
compilador genera llamadas a esas estructuras para longitud, indexación,
mutación e iteración.

```rivo
fn main():
    var xs:list<int> = [1, 2]
    xs.add(3)
    print(len(xs))
```

Los índices y claves se comprueban en ejecución; fallos no capturados terminan
el programa. Ver [listas](../collections/lists.md), [mapas](../collections/maps.md)
y [errores](errors.md).
