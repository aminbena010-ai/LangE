# Errores del runtime

El runtime informa errores fatales en operaciones inválidas. Dentro de un
bloque `try`, un error recuperable se transforma en el mensaje string de su
bloque `catch`.

```rivo
fn main():
    var xs:list<int> = []
    try:
        print(xs[0])
    catch mensaje:
        print(mensaje)
```

Si el error no se captura, el runtime informa el fallo y detiene la ejecución.
Los mensajes concretos y la ubicación pueden variar según operación.
