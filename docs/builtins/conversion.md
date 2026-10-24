# Conversión y matemáticas incorporadas

`str` convierte escalares a string; `to_int` y `to_float` analizan texto.
`bin`, `oct` y `hex` producen representaciones de enteros; `round` redondea un
float a int; `pow` y `abs` operan con enteros.

```rivo
fn main():
    var n:int = to_int("42")
    var texto:string = str(n)
    print(hex(n))
    print(texto)
```

Entrada numérica no válida produce error de runtime. También existe el módulo
[math](../stdlib/math.md).
