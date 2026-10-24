# `null`

`null` es un literal y tipo especial que representa ausencia de valor. No es
un valor centinela genérico ni sustituye cualquier tipo: comprueba con el
compilador las combinaciones de asignación admitidas antes de depender de ellas.

```rivo
fn main():
    var ausencia:null = null
    print("Se inicializó null")
```

Los mapas y listas no son automáticamente anulables por contener este tipo.
Aunque el parser y el análisis aceptan el literal, algunas operaciones de
runtime sobre `null` aún no están implementadas de forma segura; evita
compararlo o imprimirlo directamente en esta versión. Consulta las reglas de
compatibilidad en [sistema de tipos](../reference/type-system.md).
