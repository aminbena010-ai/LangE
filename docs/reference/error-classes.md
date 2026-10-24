# Clases de error

Los diagnósticos se agrupan en clases reconocibles:

| Clase habitual | Ejemplos |
| --- | --- |
| `SyntaxError` | sintaxis, `break` fuera de bucle. |
| `TypeError` | tipo incompatible, argumentos o índice inválidos. |
| `NameError` | nombre no definido o duplicado. |
| `ImportError` | módulo o símbolo no importable. |
| `AttributeError` | acceso a nombre privado. |
| `ValueError` | asignación a constante o valor inválido. |

```sh
./build/rivo programa.ro
```

Los nombres son una guía de clasificación; el mensaje concreto depende del
caso y versión. Véase [diagnósticos](../errors/diagnostics.md).
