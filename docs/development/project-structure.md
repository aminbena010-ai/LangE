# Estructura del proyecto

| Ruta | Contenido |
| --- | --- |
| `include/` | Interfaces C del compilador/runtime. |
| `src/` | Compilador y runtime. |
| `lib/` | Módulos Rivo y funciones externas C. |
| `tests/` | Pruebas de integración, errores, stdlib y runtime. |
| `extensions/` | Extensiones relacionadas con el editor. |
| `web/` | Sitio web estático del proyecto. |
| `docs/` | Esta documentación organizada por temas. |

```sh
find src include lib tests -maxdepth 2 -type f
```

El ejecutable generado por CMake está en `build/rivo`.
