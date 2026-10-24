# Diagnósticos

El compilador informa errores léxicos, sintácticos, semánticos, de importación
y de codegen con archivo y posición cuando están disponibles. Las clases
incluyen nombres como `SyntaxError`, `TypeError`, `NameError` e `ImportError`.

El siguiente fragmento es deliberadamente inválido: sirve para observar un
diagnóstico de tipos y debe ser rechazado por el compilador.

```rivo
fn main():
    var edad:int = "veinte"
```

El ejemplo se rechaza por incompatibilidad de tipos antes de enlazar. Ejecuta
el compilador en terminal para leer el mensaje y su sugerencia; usa la primera
ubicación reportada como punto de partida.
