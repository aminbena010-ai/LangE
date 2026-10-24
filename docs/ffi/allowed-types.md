# Tipos permitidos en FFI

Los tipos permitidos en parámetros y retornos de funciones externas son
los escalares siguientes. Una función sin `-> tipo` (o con `-> void`)
no devuelve valor. Las colecciones no se exponen mediante esta interfaz.

## Escalares básicos

| Tipo E++ | C equivalente | Notas |
|---|---|---|
| `int` | `long long` (int64_t) | Entero de 64 bits con signo |
| `float` | `double` | IEEE 754 double |
| `string` | `const char*` | Ver propiedad más abajo |
| `bool` | `uint8_t` (0/1) | |
| `char` | `char` | |

## Enteros de ancho fijo (FFI)

Disponibles **solo en firmas `external`**: no se pueden usar en
variables, constantes, campos de struct ni firmas de funciones normales.

| Tipo E++ | C equivalente | LLVM |
|---|---|---|
| `i8` | `int8_t` | i8 |
| `i16` | `int16_t` | i16 |
| `i32` | `int32_t` | i32 |
| `u8` | `uint8_t` | i8 |
| `u16` | `uint16_t` | i16 |
| `u32` | `uint32_t` | i32 |
| `u64` | `uint64_t` | i64 |
| `size` | `size_t` | i64 (targets de 64 bits) |

Al cruzar la frontera FFI, los enteros de ancho fijo se promueven a
`int` (i64) en el lado E++ (extensión de signo para `i8`/`i16`/`i32`,
de ceros para `u8`/`u16`/`u32`/`u64`/`size`). Al pasar un `int` a un
parámetro de ancho fijo, se estrecha (trunc) de forma segura.

## `void`

`-> void` es un retorno explícito equivalente a omitir `->`:

```rivo
external "helper.c" fn log(msg:string) -> void
external "helper.c" fn log2(msg:string)
```

## Propiedad de `string`

- `-> string` (por defecto): **borrowed** — E++ usa el puntero C tal cual;
  la cadena debe seguir siendo válida durante todo el programa (literal
  estática o memoria gestionada por C).
- `-> string owned`: E++ **copia** el resultado a su arena y libera el
  original con `free` (para cadenas `malloc`/`strdup` devueltas por C).
- `-> string static`: sinónimo explícito de borrowed, para documentar
  la intención.

## Ejemplo completo

```rivo
external "helper.c" fn es_positivo(n:int) -> bool
external "helper.c" fn etiqueta() -> string
external "helper.c" fn volumen_max() -> u8
external "helper.c" fn log(msg:string) -> void

fn main():
    print(es_positivo(4))
    print(etiqueta())
    print(volumen_max())
    log("arrancando")
```

La declaración Rivo debe coincidir exactamente con la ABI C. Una cadena
C devueltapor `-> string` (borrowed) debe permanecer válida mientras
Rivo la utilice; con `-> string owned` E++ la copia y el original se
libera.
