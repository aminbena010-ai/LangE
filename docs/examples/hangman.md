# Ejemplo: adivinar una palabra

```rivo
fn main():
    const SECRETA:string = "sol"
    var intento:string = ""
    var intentos:int = 0
    while intento != SECRETA and intentos < 3:
        print("Adivina la palabra:")
        intento = input()
        intentos += 1
    if intento == SECRETA:
        print("¡Acertaste!")
    else:
        print("Se acabaron los intentos")
```

Es un juego mínimo de palabra completa, no una implementación de dibujo o
seguimiento de letras.
