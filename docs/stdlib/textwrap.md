# Módulo estándar `textwrap`

`textwrap` envuelve o indenta texto:

```rivo
import textwrap

fn main():
    print(textwrap.fill("una frase corta para mostrar", 12))
```

API: `wrap`, `fill`, `indent`, `dedent` y `shorten`. El ancho se mide según la
lógica de caracteres implementada en el módulo, no necesariamente el ancho
visual Unicode de una terminal.
