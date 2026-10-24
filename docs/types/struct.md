# `struct`

Un `struct` declara, a nivel de módulo, un tipo compuesto de solo datos:

```rivo
struct Punto:
    var x:float
    var y:float
```

Cada campo requiere una anotación de tipo y no puede tener un valor por
defecto. Se pueden usar tipos básicos, `list<T>`, `map<K, V>` y otros structs
declarados en el mismo módulo.

## Construcción y acceso

El constructor recibe un argumento posicional por campo y respeta el orden de
la declaración:

```rivo
fn main():
    var punto:Punto = Punto(1.5, 2.5)
    punto.x = 3.0
    print(punto.y)
```

Los argumentos con nombre, métodos, herencia y constructores vacíos no están
admitidos. Un campo inexistente, una cantidad incorrecta de argumentos o un
tipo incompatible producen un error semántico.

Los structs son referencias administradas por el runtime. Asignar una variable
a otra conserva la identidad del objeto; modificar un campo a través de
cualquiera de las referencias modifica el mismo objeto. Por ello, `==` y `!=`
comparan identidad, no los valores de los campos. Los structs pueden usarse
como elementos de `list<T>`.

`print` representa un struct como `Punto(x=3.0, y=2.5)`. También admite structs
anidados, siempre que todos sus campos sean imprimibles (`int`, `float`,
`string`, `bool`, `char` u otro struct imprimible). Un campo `list`, `map` o
`null` impide imprimir el struct completo.

Los campos struct no aceptan `null`, y las definiciones recursivas están
prohibidas. Los structs de módulos no se pueden importar como tipos en el
archivo principal.
