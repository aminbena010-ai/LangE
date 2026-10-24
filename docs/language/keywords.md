# Palabras clave

Las palabras reservadas implementadas son:

`var`, `const`, `fn`, `return`, `struct`, `if`, `elif`, `else`, `while`,
`for`, `in`, `break`, `continue`, `pass`, `try`, `catch`, `import`, `from`, `as`,
`external`, `async`, `await`, `true`, `false`, `null`, `int`, `float`,
`string`, `bool`, `char`, `list`, `map`, `task`, `and`, `or`, `not`.

`async` y `await` forman parte del diseño de concurrencia cooperativa y `task`
es el tipo `task<T>` que devuelve una `async fn`. El frontend ya las acepta,
pero el codegen todavía no compila programas async: no las uses en programas
que necesites compilar.

```rivo
fn main():
    var activo:bool = true
    if activo and not false:
        pass
```

Las palabras clave son minúsculas. Los tokens de puntuación y operadores se
enumeran en [tokens](../reference/tokens.md); los tipos se explican en
[visión general](../types/overview.md).
