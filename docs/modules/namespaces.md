# Módulos y espacios de nombres

Los módulos exponen funciones públicas. Con `import m`, sus funciones se
llaman como `m.nombre()`; `from m import nombre` las lleva al espacio de
nombres local. Los nombres privados empiezan con `_`.

```rivo
import text as txt

fn main():
    print(txt.to_upper("rivo"))
```

Evita colisiones entre símbolos importados y declaraciones locales. Los
imports circulares y nombres duplicados se consideran errores.
