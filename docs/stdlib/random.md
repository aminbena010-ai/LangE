# Módulo estándar `random`

`random` ofrece `set_seed(int)`, `integer(min, max)`, `uniform()` y
`chance(p:float)`.

```rivo
import random

fn main():
    random.set_seed(10)
    print(random.integer(1, 6))
    print(random.chance(0.5))
```

Fija la semilla solo para pruebas reproducibles; no uses este generador para
secretos criptográficos.
