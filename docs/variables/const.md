# Constantes con `const`

`const nombre:tipo = expresión` declara un valor que no puede reasignarse.
Debe tener tipo explícito.

```rivo
fn main():
    const LIMITE:int = 3
    var i:int = 0
    while i < LIMITE:
        i += 1
    print(i)
```

Una asignación posterior a una constante es un error semántico. La palabra
`const` no implica evaluación en tiempo de compilación para toda expresión.
