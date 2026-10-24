# Ejemplo: Fibonacci iterativo

```rivo
fn main():
    var a:int = 0
    var b:int = 1
    var i:int = 0
    while i < 8:
        print(a)
        var siguiente:int = a + b
        a = b
        b = siguiente
        i += 1
```

La iteración evita depender de la profundidad de recursión y muestra
asignaciones de variables locales.
