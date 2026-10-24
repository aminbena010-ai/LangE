# Módulo estándar `time`

`time` proporciona relojes y esperas en milisegundos:

```rivo
import time

fn main():
    var antes:int = time.now_ms()
    time.sleep(5)
    print(time.now_ms() - antes)
```

API: `now_ms`, `unix_ms`, `ms_to_seconds`, `sleep` y `sleep_seconds`.
`now_ms` sirve para medir duración; `unix_ms` representa tiempo de época.
