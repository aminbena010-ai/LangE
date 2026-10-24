# Ejemplo: menú interactivo

```rivo
fn main():
    var opcion:string = ""
    while opcion != "q":
        print("1. Saludar | q. Salir")
        opcion = input()
        if opcion == "1":
            print("¡Hola!")
    print("Adiós")
```

`input()` devuelve texto; compara la opción con literales string.
