# De JavaScript a Rivo

Rivo no es dinámicamente tipado: cada variable tiene tipo y las colecciones
son homogéneas. No tiene `undefined`, objetos JavaScript ni coerción
automática; usa `null` explícitamente donde sea compatible.

```rivo
fn main():
    var activo:bool = true
    var nombres:list<string> = ["Ada", "Lin"]
    if activo:
        print(nombres[0])
```

Las llaves no forman bloques; usa indentación. Consulta [tipos](../types/overview.md)
y [mapas](../collections/maps.md).
