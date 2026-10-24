# Módulo estándar `system`

`system` expone información del proceso y el sistema anfitrión:

```rivo
import system

fn main():
    print(system.os())
    print(system.architecture())
```

API: `pid`, `current_directory`, `hostname`, `os`, `architecture`, `get_env`,
`has_env`, `set_env`, `username`, `cpu_count`, `total_memory_mb`,
`available_memory_mb` y `uptime`. Los resultados pueden depender de la
plataforma y permisos.
