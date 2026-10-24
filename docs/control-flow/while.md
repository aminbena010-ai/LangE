# Bucle `while`

`while condición:` repite su bloque mientras la condición booleana sea cierta.
Actualiza las variables de control para evitar un bucle infinito.

```rivo
fn main():
    var i:int = 1
    while i <= 3:
        print(i)
        i += 1
```

`break` termina el bucle y `continue` salta a la siguiente iteración:
[break y continue](break-continue.md).
