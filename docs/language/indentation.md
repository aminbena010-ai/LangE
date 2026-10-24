# Indentación y bloques

Después de `:` se espera una nueva línea y un bloque con mayor sangría. Al
volver al nivel anterior termina el bloque. Usa una sangría uniforme (cuatro
espacios es la convención recomendada); no mezcles tabuladores y espacios.

```rivo
fn main():
    var i:int = 0
    while i < 2:
        print(i)
        i += 1
    print("fin")
```

Las líneas vacías y comentarios no forman bloques. La indentación representa
estructura sintáctica, no decoración. Consulta [sintaxis](syntax.md) y
[control de flujo](../control-flow/while.md).
