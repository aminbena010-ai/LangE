# Módulo estándar `validation`

`validation` ofrece validaciones simples para texto y números:

```rivo
import validation

fn main():
    print(validation.is_email("ana@example.org"))
    print(validation.min_length("rivo", 2))
```

API: `not_empty`, `min_length`, `max_length`, `in_range`, `numeric`,
`is_float`, `letters`, `alphanumeric` e `is_email`. Son validaciones
prácticas, no sustituyen validaciones estrictas de protocolo o seguridad.
