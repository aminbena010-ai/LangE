# Parámetros

Cada parámetro usa `nombre:tipo`. Los argumentos se pasan en el orden de la
declaración; no hay parámetros por defecto ni argumentos con nombre.

```rivo
fn repetir(texto:string, veces:int) -> string:
    var resultado:string = ""
    var i:int = 0
    while i < veces:
        resultado += texto
        i += 1
    return resultado

fn main():
    print(repetir("ro", 2))
```

El compilador verifica cantidad y tipos al llamar la función.
