# `int`

`int` representa enteros con signo. Los literales se escriben en base 10 y las
operaciones aritméticas enteras producen `int`; la división entera trunca hacia
cero. No se convierte automáticamente a `float`.

```rivo
fn main():
    var total:int = 7 / 2
    print(total)
```

Usa `float` cuando necesites fracciones. Para analizar texto hay `to_int`;
consulta [conversión](../builtins/conversion.md).
