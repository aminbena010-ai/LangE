# Internals: lexer

El lexer de `src/lexer.c` convierte el flujo de caracteres en tokens de
identificador, literal, operador, palabra clave, salto de línea e indentación.
También procesa comentarios y f-strings.

```text
fn main(): → FN IDENTIFIER ( ) : NEWLINE INDENT ...
```

Para investigar un cambio léxico, actualiza las pruebas de tokens o ejemplos
de integración y conserva posiciones de línea/columna en diagnósticos.

## Límites que conviene vigilar

El lexer guarda la longitud, la posición y la columna en enteros (`int`), y
convierte a `int` la longitud obtenida con `strlen`. Por ello, un archivo fuente
que supere `INT_MAX` bytes puede desbordar esos valores; cambiar solo el tamaño
de la lectura del archivo no eliminaría el límite, porque los offsets y los
tokens también usan enteros.

La pila de indentación tiene una capacidad fija de 256 niveles. Al alcanzar ese
límite, el lexer no emite el token de indentación ni un diagnóstico específico,
por lo que el parser puede mostrar un error confuso. Si se cambia este límite,
hay que conservar la detección explícita del exceso y añadir una prueba de
regresión. El número de líneas no tiene ese tope: un programa de 1.000 líneas
se comprobó correctamente; esto no implica que archivos arbitrariamente
grandes estén soportados.
