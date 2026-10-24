# Asignación compuesta

Rivo admite `+=`, `-=`, `*=`, `/=`. Cada operador actualiza la variable de la
izquierda usando el operador aritmético correspondiente.

```rivo
fn main():
    var total:int = 10
    total -= 3
    total += 2
    print(total)
```

La asignación compuesta requiere un destino asignable y un resultado compatible
con su tipo. No existe `%=`.
