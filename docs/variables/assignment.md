# Asignación

`=` asigna a una variable existente. Las formas compuestas disponibles son
`+=`, `-=`, `*=`, `/=` y equivalen a operar y guardar el resultado.

```rivo
fn main():
    var n:int = 8
    n += 2
    n *= 3
    print(n)
```

El tipo de la expresión asignada debe ser compatible con el destino; las
constantes no pueden reasignarse. `==` es comparación, no asignación.
