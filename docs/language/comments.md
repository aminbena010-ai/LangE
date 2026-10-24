# Comentarios

Una línea cuyo primer contenido sea `#` es un comentario. También se admiten
bloques delimitados por `#!` y `!#`.

```rivo
fn main():
    # comentario de una línea
    #!
    Nota de varias líneas.
    !#
    print("ok")
```

Los bloques no se anidan; cierra cada bloque con `!#`. No uses `//` como
comentario. En esta implementación los comentarios se ignoran durante el
análisis léxico; no cambian el valor de una expresión.
