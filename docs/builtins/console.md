# Consola

Las funciones incorporadas de consola son `print(...)` e `input()`.
`print` acepta uno o más escalares (`int`, `float`, `string`, `bool`, `char`),
los separa con espacios y añade una nueva línea. `input` lee una línea y
devuelve `string`; también puede recibir un prompt opcional.

```rivo
fn main():
    print("¿Cómo te llamas?")
    var nombre:string = input()
    print(f"Hola, {nombre}")
    print("Edad:", 20, true)
```

En fin de entrada, `input()` devuelve una cadena vacía. Para entrada carácter
a carácter, consulta [terminal](terminal.md).
