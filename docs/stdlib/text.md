# Módulo estándar `text`

El módulo `text` ofrece operaciones de texto con nombres explícitos:

```rivo
import text

fn main():
    print(text.to_upper("rivo"))
    print(text.combine(text.parts("a,b", ","), " "))
```

API: `to_upper`, `to_lower`, `strip`, `parts`, `combine`, `substitute`,
`includes`, `has_prefix`, `has_suffix`, `index_of`, `slice`, `is_empty`,
`repeat`, `count`, `capitalize`, `reverse` y `collapse_spaces`. Las posiciones
y transformaciones se basan en bytes/ASCII, no en un modelo completo de
graphemes Unicode.
