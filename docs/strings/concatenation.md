# Concatenar strings

El operador `+` crea texto concatenando strings. También está disponible
`concat(a, b)`.

```rivo
fn main():
    var saludo:string = "Hola, " + "Rivo"
    print(saludo)
```

Los tipos deben ser strings; convierte valores con `str` si necesitas
combinarlos. Para inserción legible en contexto, usa [f-strings](f-strings.md).
