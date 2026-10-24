# Referencia de tokens y operadores

El lexer reconoce identificadores, literales enteros/float/string/char,
tokens de f-string, palabras clave, indentación (`INDENT`, `DEDENT`), nueva
línea y fin de archivo.

| Familia | Formas |
| --- | --- |
| Aritmética | `+ - * / // % ++ --` |
| Asignación | `= += -= *= /=` |
| Comparación | `== != < > <= >=` |
| Lógica | `and or not` |
| Puntuación | `: , . -> ( ) [ ] { }` |

```rivo
fn main():
    var n:int = 3
    print(f"n={n}")
```

En f-strings `{` y `}` delimitan interpolación; en literales de mapa delimitan
entradas. No son delimitadores de bloque.
