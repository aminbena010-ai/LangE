# Sintaxis

Un fuente Rivo consta de declaraciones y sentencias separadas por saltos de
línea. Palabras clave y tipos se escriben en minúscula. Los bloques se abren
con `:` y se delimitan mediante sangría; los comentarios comienzan con `#`.

```rivo
fn main():
    var saldo:int = 10
    if saldo > 0:
        print("Disponible")
    else:
        print("Vacío")
```

Las llamadas usan paréntesis, las listas y mapas corchetes, y el acceso a
elementos usa `[]`. Las llaves `{}` solo se usan para expresiones interpoladas
en f-strings (o escapadas como `{{`/`}}`). No hay `switch` ni continuación
de expresiones entre líneas documentada; las condicionales encadenadas usan
`elif`.

Los tipos de datos propios se declaran con `struct`; consulta la
[referencia de structs](../types/struct.md).

Ver [indentación](indentation.md), [comentarios](comments.md) e
[identificadores](identifiers.md).
