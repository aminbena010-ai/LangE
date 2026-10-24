# Módulo estándar `strings`

`strings` proporciona conjuntos de caracteres y transformaciones:

```rivo
import strings

fn main():
    print(strings.digits())
    print(strings.capwords("hola mundo"))
```

API: `digits`, `ascii_lowercase`, `ascii_uppercase`, `ascii_letters`,
`hex_digits`, `punctuation`, `whitespace`, `lstrip`, `rstrip`, `swapcase` y
`capwords`. Las constantes de caracteres son ASCII.
