# Hola, mundo

Crea `hola.ro`:

```rivo
fn main():
    print("Hola, mundo")
```

Compila y ejecuta desde la raíz del proyecto:

```sh
./build/rivo hola.ro -o hola
./hola
```

`fn` declara una función, `main` es el punto de entrada y `print` escribe el
texto seguido de una nueva línea. El cuerpo lleva cuatro espacios de sangría.
Consulta [declaraciones de función](../functions/declaration.md) y
[consola](../builtins/console.md).
