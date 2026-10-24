# Operadores lógicos

`and`, `or` y `not` operan sobre valores `bool`. `not` es unario; los otros
dos combinan condiciones.

```rivo
fn main():
    var edad:int = 20
    var tiene_entrada:bool = true
    if edad >= 18 and tiene_entrada:
        print("Puede entrar")
```

Agrupa las expresiones con paréntesis si mejora la lectura. La precedencia
sitúa `and` por encima de `or`; consulta [precedencia](precedence.md).
