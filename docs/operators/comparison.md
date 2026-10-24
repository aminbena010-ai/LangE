# Comparaciones

Los operadores de comparación son `==`, `!=`, `<`, `>`, `<=` y `>=`.
Producen `bool`. Las cadenas admiten igualdad, desigualdad y orden
lexicográfico (`<`, `>`, `<=`, `>=`) desde la versión 0.3.0. Listas y mapas
no admiten igualdad estructural.

```rivo
fn main():
    var edad:int = 18
    print(edad >= 18)

    var a:string = "abc"
    var b:string = "abd"
    print(a < b)       # true (orden lexicográfico)
    print(a == "abc")  # true
```

Para probar equivalencia usa `==`, nunca `=`; combina condiciones con
[operadores lógicos](logical.md).
