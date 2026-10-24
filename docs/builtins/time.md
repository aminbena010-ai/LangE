# Tiempo incorporado

`time_ms()` devuelve milisegundos monotónicos para medir intervalos;
`epoch_ms()` devuelve milisegundos desde la época Unix. `sleep_ms(ms)` pausa
la ejecución.

```rivo
fn main():
    var inicio:int = time_ms()
    sleep_ms(10)
    print(time_ms() - inicio)
```

Usa [módulo time](../stdlib/time.md) para las funciones relacionadas.
