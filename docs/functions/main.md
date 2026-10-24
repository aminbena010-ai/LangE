# Punto de entrada `main`

El programa ejecutable necesita una función `main` en el archivo principal.
Los módulos importados no deben definirla.

```rivo
fn main():
    print("Inicio")
```

`main` suele declararse sin parámetros ni tipo de retorno. Guarda el código
como `.ro` y compílalo con `rivo archivo.ro -o salida`.
