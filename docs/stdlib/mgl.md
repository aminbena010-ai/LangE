# mgl

Librería de ventanas gráficas para Linux (X11 + Xft) con estilo Tk: se crean
widgets con IDs enteros y los eventos se leen bombeando en un bucle (E++ no
tiene callbacks).

Requisitos del sistema (headers de desarrollo): `libx11-dev`, `libxft-dev`,
`libfontconfig1-dev`. Enlace:

```sh
rivo programa.ro -l X11 -l Xft -l fontconfig
```

## Ejemplo

```rivo
import mgl

fn main():
    var v:int = mgl.window("Hola", 320, 200)
    var b:int = mgl.button(v, "Pulsa", 20, 60, 120, 36)
    var e:int = mgl.entry(v, 20, 110, 200, 32, 64)

    while mgl.update():
        if mgl.event_type() == mgl.event_click():
            if mgl.event_widget() == b:
                mgl.config(e, "text", "pulsado")
```

## API

### Ventana y aplicación

| Función | Descripción |
|---|---|
| `available() -> bool` | ¿Conecta con el display? |
| `window(titulo, ancho, alto) -> int` | Nueva ventana (ID o 0) |
| `title(ventana, titulo)` | Cambia el título |
| `geometry(ventana, ancho, alto)` | Redimensiona la ventana |
| `resizable(ventana, ancho, alto)` | `false` fija ese eje (no se puede redimensionar) |
| `background(ventana, r, g, b)` | Color de fondo de la ventana |
| `destroy(ventana) -> bool` | Cierra y destruye |
| `windows_open() -> int` | Ventanas vivas |
| `font(nombre, tamano)` | Fuente global (fontconfig) |

### Widgets

Todos devuelven el ID (0 si falla): `label(ventana, texto, x, y)`,
`button(ventana, texto, x, y, ancho, alto)`,
`entry(ventana, x, y, ancho, alto, largo_max)`,
`checkbutton(ventana, texto, x, y)`, `frame(ventana, x, y, ancho, alto)`.

### Métodos de widget

| Función | Descripción |
|---|---|
| `config(id, opcion, valor) -> bool` | Ver opciones abajo; `false` si id/opción no válidos |
| `get(id) -> string` | Texto actual; `"1"`/`"0"` en casillas |
| `insert(id, texto)` | Añade texto al final |
| `delete(id, inicio, fin)` | Borra `[inicio, fin)`; `fin < 0` borra hasta el final |

Opciones de `config`: `"text"` (texto), `"fg"` y `"bg"` (`"r,g,b"`),
`"state"` (`"normal"` o `"disabled"`), `"checked"` (`"true"`/`"false"`, solo
casillas).

### Eventos

| Función | Descripción |
|---|---|
| `update() -> bool` | Bombea sin bloquear; `false` = sin ventanas |
| `wait() -> bool` | Bombea hasta un evento |
| `event_type() / event_widget() / event_text() / event_key() / event_x() / event_y()` | Último evento |

Tipos de evento: `event_none()` 0, `event_click()` 1, `event_keypress()` 2,
`event_return()` 3 (Intro en entrada), `event_check()` 4, `event_close()` 5.

## Limitaciones (v1)

- Teclado ASCII de entrada; sin IME ni Unicode en entradas.
- Títulos ASCII.
- X11 (en Wayland funciona vía XWayland); sin backend Wayland nativo.
- Sin display, las funciones devuelven 0/false sin abortar.
- `insert`/`delete` solo sobre entradas (y `insert` también en etiquetas y
  botones); no hay cursor ni selección.
