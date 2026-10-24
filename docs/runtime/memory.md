# Memoria del runtime

El código generado usa el runtime nativo de Rivo. Los valores de colección,
texto y `struct` se administran mediante estructuras internas; los programas
`.ro` no liberan memoria manualmente. Los structs y las colecciones se liberan
al terminar el programa.

```rivo
fn crear_mensaje() -> string:
    return "memoria administrada"

fn main():
    print(crear_mensaje())
```

Las reglas exactas de vida útil dependen del runtime y de la versión. En FFI,
respeta las condiciones especiales para strings devueltos por C:
[tipos FFI](../ffi/allowed-types.md).
