# Condicionales `if`, `elif` y `else`

La condición de `if` debe producir `bool`. `else` es opcional; `elif`
encadena condiciones adicionales sin anidar `if`.

```rivo
fn main():
    var nota:int = 75
    if nota >= 90:
        print("Excelente")
    elif nota >= 60:
        print("Aprobado")
    else:
        print("Reintentar")
```

Cada rama es un bloque indentado. `elif` puede repetirse las veces que
haga falta; `else`, como mucho, va al final. No existe `elseif`. Tras un
`else` no puede venir un `elif` (error de sintaxis).

`elif` es azúcar sintáctico: el compilador lo convierte en un `if` anidado
dentro del bloque `else`, por lo que el comportamiento es idéntico a
anidar condicionales a mano, solo que más legible.
