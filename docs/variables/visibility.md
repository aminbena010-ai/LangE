# Visibilidad de nombres

En módulos, los nombres públicos se importan desde otros archivos. Un nombre
que comienza por `_` es privado y no se puede importar desde fuera.

```rivo
# modulo.ro
fn doble(n:int) -> int:
    return n * 2

fn _interno(n:int) -> int:
    return n + 1
```

`from modulo import doble` es válido; importar `_interno` no lo es. La
visibilidad no reemplaza la comprobación de tipos. Consulta [namespaces](../modules/namespaces.md).
