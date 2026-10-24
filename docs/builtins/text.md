# Funciones incorporadas de texto

Funciones incorporadas: `concat`, `split`, `join`, `upper`, `lower`, `trim`,
`substring`, `replace`, `contains`, `starts_with`, `ends_with`, `find`,
`ord` y `chr`.

```rivo
fn main():
    var partes:list<string> = split("ro-lang", "-")
    print(join(partes, " "))
    print(upper("rivo"))
```

`input()` lee una línea y devuelve un string; también acepta un prompt opcional
de tipo string, que se muestra sin salto de línea antes de leer:

```rivo
fn main():
    var nombre:string = input("Nombre: ")
    print(nombre)
```

`substring(texto, inicio, longitud)` usa posiciones de bytes. `find` devuelve
`-1` si no encuentra el fragmento. `ord` recibe string y `chr` devuelve un
string de un carácter. Consultar [strings](../strings/literals.md).
