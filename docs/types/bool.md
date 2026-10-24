# `bool`

`bool` tiene los valores `true` y `false`. Las comparaciones y operadores
lógicos producen booleanos. Las condiciones de `if` y `while` deben ser
booleanas.

```rivo
fn mayor_de_edad(edad:int) -> bool:
    return edad >= 18

fn main():
    print(mayor_de_edad(20))
```

Los operadores son `and`, `or` y `not`; consulta [lógica](../operators/logical.md).
