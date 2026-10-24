# Identificadores

Los identificadores nombran funciones, parámetros y variables. Deben comenzar
con una letra o `_`; después pueden incluir letras, dígitos y `_`. Las palabras
reservadas no son nombres disponibles.

```rivo
fn _duplicar(valor:int) -> int:
    return valor * 2

fn main():
    var resultado:int = _duplicar(4)
    print(resultado)
```

Los nombres con `_` inicial son privados en el contexto de módulos. Mantén
nombres consistentes; consulta [visibilidad de módulos](../modules/namespaces.md).
