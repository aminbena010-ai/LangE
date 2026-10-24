# Filosofía y convenciones

Rivo combina la sintaxis legible de bloques indentados con declaraciones
tipadas y compilación nativa. La sintaxis del lenguaje usa palabras clave en
inglés (`fn`, `var`, `if`, `while`); los identificadores pueden nombrarse como
prefiera el proyecto.

- Declara variables con tipo: `var edad:int = 20`.
- Delimita bloques con `:` y sangría; no uses llaves para bloques.
- Prefiere funciones pequeñas y tipos compatibles explícitos.
- El prefijo `_` marca nombres privados en módulos.

No hay conversión implícita entre `int` y `float`. Los recursos de memoria se
gestionan por el runtime; el usuario no llama a `free`.

```rivo
fn es_par(n:int) -> bool:
    return n % 2 == 0

fn main():
    print(es_par(8))
```
