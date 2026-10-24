# Retorno de funciones

`return expresión` entrega el resultado de una función. Su tipo debe coincidir
con el tipo declarado después de `->`. En procedimientos, no retornes un valor.

```rivo
fn absoluto(n:int) -> int:
    if n < 0:
        return -n
    return n

fn main():
    print(absoluto(-5))
```

Usa rutas de retorno claras en cada rama; la cobertura exacta de todas las
rutas se valida durante compilación y pruebas.
