# Módulo estándar `paths`

`paths` manipula rutas como texto; no accede al sistema de archivos:

```rivo
import paths

fn main():
    print(paths.join("datos", "entrada.csv"))
    print(paths.extension("datos/entrada.csv"))
```

API: `join`, `name`, `directory`, `extension`, `without_extension`,
`name_without_extension`, `is_absolute`, `components` y `normalize`. Sus
separadores y reglas pueden variar según las convenciones implementadas.
