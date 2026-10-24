# Aleatoriedad incorporada

`random(min, max)` devuelve un entero dentro del intervalo inclusivo;
`random_float()` produce un float en `[0, 1)`; `seed(entero)` establece la
semilla del generador.

```rivo
fn main():
    seed(7)
    print(random(1, 6))
    print(random_float())
```

Los límites invertidos son un error de runtime. Para API adicional consulta
[módulo random](../stdlib/random.md).
