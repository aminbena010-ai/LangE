# Módulo estándar `statistics`

Las funciones estadísticas trabajan con listas de `float`:

```rivo
import statistics

fn main():
    var datos:list<float> = [1.0, 2.0, 3.0]
    print(statistics.mean(datos))
    print(statistics.median(datos))
```

API: `sum`, `product`, `mean`, `min_list`, `max_list`, `range`, `median`,
`mode`, `variance`, `standard_deviation` y `percentile`. Revisa
`lib/statistics.ro` para el tratamiento de listas vacías y entradas
degeneradas.
