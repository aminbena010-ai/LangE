# Precedencia de operadores

De menor a mayor:

1. Asignación `=`, `+=`, `-=`, `*=`, `/=`.
2. `or`.
3. `and`.
4. Comparaciones `==`, `!=`, `<`, `>`, `<=`, `>=`.
5. `+`, `-`.
6. `*`, `/`, `//`, `%`.
7. Prefijo `++x`, `--x`, unarios `-`, `not`.
8. Sufijo `x++`, `x--`.

`++` y `--` solo modifican variables mutables de tipo `int`. En forma prefija
devuelven el valor actualizado; en forma sufija devuelven el valor anterior.

```rivo
fn main():
    var resultado:int = 2 + 3 * 4
    print(resultado)
```

El ejemplo equivale a `2 + (3 * 4)`. Usa paréntesis cuando la agrupación no sea
obvia. La cadena de comparación no sustituye comprobaciones de tipo.
