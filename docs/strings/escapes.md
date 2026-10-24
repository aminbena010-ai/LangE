# Secuencias de escape

Dentro de strings, f-strings y chars se reconocen:

| Escape | Valor |
| --- | --- |
| `\n` | salto de línea |
| `\r` | retorno de carro |
| `\t` | tabulador |
| `\0` | byte nulo |
| `\\` | barra inversa |
| `\"` | comilla doble |
| `\'` | comilla simple |

```rivo
fn main():
    print("primera\nsegunda")
```

Un escape desconocido o sin cerrar produce error léxico.
