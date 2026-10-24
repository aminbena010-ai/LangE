# `map<K, V>`

`map<K, V>` almacena valores de tipo `V` asociados a claves de tipo `K`. La
implementación admite literales y acceso por índice entre corchetes.

```rivo
fn main():
    var edades:map<string, int> = ["Ada": 36]
    edades["Lin"] = 25
    print(edades["Ada"])
```

Una clave ausente al leerla produce un error de runtime; asignarla la inserta o
actualiza. `len(mapa)` y `mapa.len()` (0.3.0) cuentan las entradas, y
`mapa.remove(clave)` elimina una entrada (sin efecto si la clave no existe);
no hay más métodos de mapa. No uses llaves para crear mapas. Consulta
[mapas](../collections/maps.md).
