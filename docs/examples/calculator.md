# Ejemplo: calculadora sencilla

```rivo
fn sumar(a:int, b:int) -> int:
    return a + b

fn restar(a:int, b:int) -> int:
    return a - b

fn main():
    print(sumar(8, 3))
    print(restar(8, 3))
    print(8 * 3)
    print(8 / 3)
```

Compila con `rivo calculadora.ro -o calculadora`; la división entre enteros
trunca el resultado.
