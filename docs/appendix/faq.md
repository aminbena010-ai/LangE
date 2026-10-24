# Preguntas frecuentes

### ¿Por qué falla `decimal` como tipo?

El tipo de fuente se llama `float`; `decimal` no es una palabra clave.
Para convertir texto usa `to_float`.

### ¿Rivo usa llaves para bloques?

No. Los bloques usan `:` e indentación; los literales de mapa admiten
llaves o corchetes.

### ¿Cómo añado un módulo?

Guárdalo como `.ro` junto al archivo principal o añade su directorio con `-I`.

### ¿Puedo llamar C?

Sí, declara la firma con `external`; consulta la [guía FFI](../ffi/external.md).

```rivo
fn main():
    print("Comprueba también la versión del compilador")
```
