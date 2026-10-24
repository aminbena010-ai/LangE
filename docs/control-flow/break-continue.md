# `break` y `continue`

`break` sale del bucle más interno. `continue` comienza la siguiente iteración.
Ambas sentencias solo son válidas dentro de un bucle.

```rivo
fn main():
    var i:int = 0
    while i < 5:
        i += 1
        if i == 2:
            continue
        if i == 4:
            break
        print(i)
```

Usarlas fuera de `while` o `for` genera un error de sintaxis/semántica.
