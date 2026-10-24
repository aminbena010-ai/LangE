# `float`

`float` es el tipo numérico con parte fraccionaria y se representa como
precisión doble en el runtime. Un literal como `2.5` tiene tipo `float`.
La asignación exige tipo compatible; `5` no se convierte por sí solo a `5.0`.

```rivo
fn main():
    var mitad:float = 7.0 / 2.0
    print(mitad)
```

Para convertir texto a `float` usa `to_float`.
