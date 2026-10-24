# Ejemplo: contar palabras

```rivo
import text

fn main():
    var frase:string = "uno dos uno"
    var palabras:list<string> = text.parts(frase, " ")
    print(len(palabras))
```

Cuenta elementos separados por espacios exactos; para entrada arbitraria,
normaliza el texto y trata separadores consecutivos según las necesidades de
la aplicación.
