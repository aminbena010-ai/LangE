# Módulo estándar `color`

`color` construye strings con secuencias ANSI:

```rivo
import color

fn main():
    print(color.green("Correcto"))
    print(color.bold("Importante"))
```

API: `colorize`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`,
`black`, `gray`, `bold`, `underline`, `invert`, `reset`, `color_256`,
`color_256_fondo`, `color_rgb`, `color_rgb_fondo` y `no_color`. La terminal
debe admitir ANSI para mostrar formato.
