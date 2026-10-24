# Hoja rápida

```rivo
fn doble(n:int) -> int:
    return n * 2

fn main():
    const LIMITE:int = 3
    var i:int = 0
    while i < LIMITE:
        print(f"resultado={doble(i)}")
        i += 1
```

- Tipos: `int`, `float`, `string`, `bool`, `char`, `list<T>`, `map<K, V>`.
- Bloques: `:` más indentación.
- Funciones: `fn nombre(arg:tipo) -> tipo:`.
- Colecciones: listas con `[]`; mapas con `[]` o `{}`; índices desde cero.
- Importaciones: `import modulo`, `from modulo import funcion`.
- Asignaciones compuestas: `+=`, `-=`, `*=`, `/=`.
