# Ejemplo de interoperabilidad C

Una función C sin parámetros:

```c
const char *saludo_c(void) {
    return "Hola desde C";
}
```

Declárala en Rivo y úsala desde `main`:

```rivo
external "saludo.c" fn saludo_c() -> string

fn main():
    print(saludo_c())
```

El puntero devuelto debe seguir siendo válido mientras el programa lo use;
una cadena literal estática satisface esa condición. No devuelvas un buffer
local de pila.
