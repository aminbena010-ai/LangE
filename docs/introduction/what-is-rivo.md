# ¿Qué es Rivo?

Rivo es un lenguaje compilado cuyo compilador, escrito en C, traduce programas
a LLVM IR y los enlaza con un runtime nativo. El código usa bloques por
indentación y anotaciones de tipo explícitas.

```rivo
fn doble(n:int) -> int:
    return n * 2

fn main():
    print(doble(21))
```

La implementación está en evolución. El lenguaje no debe confundirse con una
especificación independiente: consulta el código, las pruebas y las notas de
cada característica. La extensión habitual del fuente es `.ro`; el ejecutable
del compilador se llama `rivo`.

Ver [filosofía](philosophy.md), [instalación](installation.md) y [ruta de
aprendizaje](../guides/learning-path.md).
