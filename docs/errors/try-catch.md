# Capturar errores con `try` y `catch`

`try:` ejecuta un bloque; si se produce un error fatal de runtime, la ejecución
salta a `catch nombre:`. El nombre del catch recibe el mensaje como `string`.
Si no ocurre error, el catch se omite.

```rivo
fn main():
    var datos:list<int> = [7]
    try:
        print(datos[9])
    catch error:
        print(error)
```

Los errores no capturados terminan el programa. `try/catch` permite un bloque
catch por `try`; prueba específicamente qué operaciones pueden fallar.
