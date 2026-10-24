# Importar un módulo

Un módulo es un archivo `.ro`. `import modulo` permite referenciar sus funciones
públicas por el nombre cualificado del módulo.

```rivo
import math

fn main():
    print(math.square_root(81.0))
```

El compilador busca el archivo correspondiente según las rutas de módulos y lo
incorpora al programa. Los imports se declaran a nivel de módulo, no dentro de
funciones. Ver [from-import](from-import.md) y [búsqueda](module-search.md).
