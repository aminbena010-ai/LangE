# Módulo estándar `csv`

El módulo `csv` convierte entre texto CSV y matrices de campos:

```rivo
import csv

fn main():
    var filas:list<list<string>> = csv.parse("a,b\nc,d")
    print(csv.columns(filas))
```

API: `quote`, `unquote`, `parse`, `row_to_string`, `format` y `columns`.
Comprueba el comportamiento con campos vacíos, comillas y saltos de línea
antes de procesar CSV arbitrario.
