# Módulo estándar `difflib`

`difflib` compara texto y genera diferencias:

```rivo
import difflib

fn main():
    print(difflib.ratio("gato", "gatos"))
```

API: `ratio`, `unified_diff` y `get_close_matches`. La lista de líneas de un
diff se proporciona como `list<string>`.
