# Sistema de tipos

Los tipos se anotan en variables, parámetros y retornos. La semántica comprueba
asignaciones, argumentos, operaciones e índices antes de generar LLVM.

```rivo
fn duplicar(n:int) -> int:
    return n * 2

fn main():
    var x:int = duplicar(4)
    print(x)
```

Los tipos básicos son `int`, `float`, `string`, `bool`, `char` y `null`;
`list<T>` y `map<K, V>` son paramétricos. El tipo `task<T>` describe el
resultado de una `async fn`; el frontend lo acepta, pero el codegen todavía no
compila programas async. No hay conversión numérica
implícita; el conjunto exacto de operaciones compatibles está definido por la
semántica implementada.
