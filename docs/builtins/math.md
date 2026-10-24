# Matemáticas incorporadas

Funciones disponibles sin import: `abs(int)`, `pow(int, int)` y `round(float)`.
Para funciones trigonométricas y matemáticas adicionales, importa el módulo
`math`.

```rivo
fn main():
    print(abs(-9))
    print(pow(2, 5))
    print(round(3.6))
```

Los tipos de argumento se comprueban estáticamente. Ver
[biblioteca math](../stdlib/math.md) y [conversiones](conversion.md).
