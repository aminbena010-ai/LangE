# Terminal

Las funciones `clear()`, `move(fila, columna)`, `sleep_ms(ms)` y `read_key()`
sirven para interacciones sencillas con terminal. `read_key` devuelve `char`;
la secuencia exacta depende de la plataforma y del terminal.

```rivo
fn main():
    print("Pulsa una tecla")
    var tecla:char = read_key()
    print(tecla)
```

`clear` y `move` emiten secuencias ANSI; en salidas redirigidas quizá no tengan
el efecto esperado. Para línea completa usa `input()`.
