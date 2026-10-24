# Internals: análisis semántico

`src/semantic.c` administra símbolos y ámbitos, revisa nombres, visibilidad,
tipos, argumentos, retornos y uso válido de saltos/errores.

```rivo
fn doble(n:int) -> int:
    return n * 2
```

El ejemplo permite verificar cómo una declaración tipada se valida antes de
generar IR. Añadir un tipo u operación requiere coherencia entre semántica y
codegen.
