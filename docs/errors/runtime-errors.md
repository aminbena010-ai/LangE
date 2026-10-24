# Errores de runtime

Los fallos durante la ejecución pueden interrumpir el programa o ser
capturados por `try/catch`. Ejemplos incluyen división por cero, índice de lista
inválido, clave inexistente y errores de entrada/salida.

```rivo
fn main():
    var datos:list<int> = [1]
    try:
        print(datos[3])
    catch problema:
        print(problema)
```

No asumas que un error implica un valor de retorno por defecto. Maneja los
casos esperables explícitamente y deja propagarse los no recuperables.
