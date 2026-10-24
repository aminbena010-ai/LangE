# Módulo estándar `json`

La API actual codifica y decodifica mapas planos de `string` a `string`:

```rivo
import json

fn main():
    var datos:map<string, string> = ["ciudad": "Lima"]
    var serializado:string = json.encode(datos)
    print(serializado)
```

API: `encode`, `decode`, `escape`, `unescape`. No presupongas soporte para
listas u objetos JSON anidados; un documento inválido puede causar un error de
runtime.
