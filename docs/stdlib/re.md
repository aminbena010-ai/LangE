# Módulo estándar `re`

`re` incluye funciones de búsqueda de patrones:

```rivo
import re

fn main():
    print(re.test("[0-9]+", "rivo42"))
```

API: `match`, `search`, `test`, `findall`, `sub` y `split`. La gramática
admitida depende de la implementación en `lib/re.ro`; no asumas compatibilidad
total con una biblioteca regex concreta.
