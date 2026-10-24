# Módulo estándar `codecs`

`codecs` ofrece conversiones Base64, hexadecimal, codificación URL y ROT13:

```rivo
import codecs

fn main():
    var codificado:string = codecs.hex_encode("Rivo")
    print(codecs.hex_decode(codificado))
```

API: `base64_encode`, `base64_decode`, `hex_encode`, `hex_decode`,
`url_quote`, `url_unquote` y `rot13`. Valida entrada externa; estas
transformaciones no son cifrado.
