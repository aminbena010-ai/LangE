# Compilar y enlazar código C

El compilador invoca `clang` para compilar los ficheros C listados mediante
`external`, y enlaza los objetos generados con el runtime de Rivo. Las rutas
relativas se interpretan desde el `.ro` que declara la función.

```c
/* helper.c */
long long triple(long long n) {
    return n * 3;
}
```

```rivo
external "helper.c" fn triple(n:int) -> int
fn main():
    print(triple(7))
```

Compila el `.ro` normalmente; usa `-I directorio` si el código C necesita
cabeceras allí. La firma de la declaración debe reflejar los tipos C usados
por el runtime de esta versión.
