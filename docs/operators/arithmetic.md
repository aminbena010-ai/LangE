# Operadores aritméticos

Operadores: `+`, `-`, `*`, `/`, `//` y `%`. Los operandos deben tener tipos
numéricos compatibles; `+` también concatena strings y listas compatibles.

```rivo
fn main():
    var cociente:int = 7 / 2
    var resto:int = 7 % 2
    var cociente_piso:int = -7 // 2
    print(cociente)
    print(resto)
    print(cociente_piso)
```

La división entera trunca hacia cero. La división por cero es un error de
runtime. `//` acepta dos valores `int` y redondea hacia menos infinito
(`-7 // 2` da `-4`); también produce `ZeroDivisionError` si el divisor es cero.
Usa paréntesis para dejar clara la agrupación; véase
[precedencia](precedence.md).

Los operadores enteros `+`, `-`, `*`, `/` y `%` se comprueban en runtime: un
resultado fuera del rango de `int` produce `OverflowError` y un divisor cero
produce `ZeroDivisionError` (también en `/` y `%`, no solo en `//`). Con un
`try` activo el error se captura en el `catch`.
