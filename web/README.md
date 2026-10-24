# Web de Rivo

Sitio web estático del proyecto: HTML + CSS + JavaScript vanilla, sin build,
sin frameworks y sin servidor propio.

Páginas:

- `index.html` — presentación del lenguaje.
- `docs.html` — documentación oficial (sustituye a la antigua wiki).
- `python.html` — guía de equivalencias Python → Rivo.
- `godot.html` — guía honesta sobre Godot/GDScript y Rivo.
- `c.html` — guía de conversión de C a Rivo.
- `cpp.html` — guía de conversión de C++ a Rivo.
- `styles.css`, `app.js` — estilos y comportamiento compartidos.

## Cómo verla

Cualquier servidor estático sirve, o incluso abrir los ficheros directamente:

```sh
# opción A: abrir index.html en el navegador
xdg-open web/index.html

# opción B: servidor estático temporal
python3 -m http.server 8000 --directory web
```

Luego abre <http://127.0.0.1:8000>.

El sitio no ejecuta código Rivo: para compilar y probar programas usa el
compilador de la raíz (`build/rivo`) según la documentación.
