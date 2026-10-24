# Declarar funciones

Una función empieza con `fn`, recibe parámetros tipados y opcionalmente declara
el tipo de retorno mediante `-> tipo`.

```rivo
fn sumar(a:int, b:int) -> int:
    return a + b

fn main():
    print(sumar(2, 3))
```

Una función sin tipo de retorno es un procedimiento y no devuelve valor. El
cuerpo debe estar indentado. Véase [parámetros](parameters.md) y [return](return.md).
