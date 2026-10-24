# Visibilidad de funciones

Las funciones exportadas por un módulo son públicas salvo que su nombre
comience con `_`. Las funciones privadas solo sirven dentro de ese módulo.

```rivo
fn suma_uno(n:int) -> int:
    return _interno(n)

fn _interno(n:int) -> int:
    return n + 1
```

Importa funciones públicas con `from modulo import suma_uno`; consulta
[visibilidad de variables](../variables/visibility.md).
