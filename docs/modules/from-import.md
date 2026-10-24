# `from ... import`

Importa funciones públicas específicas y llámalas sin prefijo de módulo:

```rivo
from math import square_root

fn main():
    print(square_root(25.0))
```

Se admiten varios nombres separados por comas. Los nombres privados (prefijo
`_`) no se pueden importar. Evita colisiones con otros nombres del ámbito.
