# Internals: sistema de errores

El compilador conserva clase, mensaje y ubicación para producir diagnósticos.
El runtime reporta fallos de ejecución y permite propagar mensajes a `catch`
en los casos soportados.

```rivo
fn main():
    try:
        var xs:list<int> = []
        print(xs[0])
    catch error:
        print(error)
```

Al cambiar errores, revisa lexer/parser, semántica, codegen y la presentación
en `src/error.c`; los casos negativos están en `tests/errors/`.
