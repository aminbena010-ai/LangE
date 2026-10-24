# Propiedad de strings en FFI

Cuando una función C devuelve un `char *`, hay que decidir
quién es dueño de la memoria.

## Modos

### `borrowed` (por defecto)

La cadena apunta a memoria que C no libera (literal, buffer
estático, memoria del runtime). No se copia.

```rivo
external "lib.c" fn getenv(name:string) -> string
```

### `static`

Igual que `borrowed`, pero documenta explícitamente que la
cadena es un literal estático de larga duración.

```rivo
external "lib.c" fn getenv(name:string) -> string static
```

### `owned`

C devuelve memoria que debe liberarse con `free`. El
compilador copia la cadena a la arena E++ y libera la
original automáticamente.

```rivo
external "lib.c" fn strdup(s:string) -> string owned
```

## Reglas

- `owned` y `static` solo se pueden usar con retorno `string`.
- Son mutuamente excluyentes: una función no puede ser
  `owned` y `static` a la vez.
- `owned` es la única forma de recibir memoria dinámica de C
  sin fugas: el `free` se emite automáticamente tras la copia.

## Ejemplo completo

```rivo
external "../fixtures/strings.c" fn saludo_dinamico(
    nombre:string
) -> string owned

fn main():
    # El malloc de C se copia a la arena y se libera solo.
    print(saludo_dinamico("E++"))  # hola E++
```
