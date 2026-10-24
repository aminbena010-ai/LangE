# Callbacks FFI

Los callbacks permiten pasar una función E++ como puntero a
función C a una función externa.

## Sintaxis

```rivo
external "lib.c" fn sum_squares(
    n:int,
    f:callback<int> -> int
) -> int
```

El tipo `callback<T1,T2,...> -> T` se usa únicamente como
parámetro de una función `external`. No puede ser el retorno
de una función externa, ni usarse en variables o campos de
struct.

## Restricciones

- El callback solo puede ser una función E++ con firma
  compatible (mismo número y tipo de parámetros, mismo retorno).
- El retorno del callback debe ser un tipo escalar (`int`,
  `float`, `bool`, `char`, `string`), no `list`, `map` ni
  `struct`.
- Los parámetros del callback deben ser tipos escalares o
  `string`.
- Un callback no puede ser `async`.

## Ejemplo completo

```rivo
external "../fixtures/callbacks.c" fn sum_squares(
    n:int,
    f:callback<int> -> int
) -> int

fn cuadrado(x:int) -> int:
    return x * x

fn main():
    print(sum_squares(5, cuadrado))  # 30
```

## Código C correspondiente

```c
int64_t sum_squares(int64_t n, int64_t (*f)(int64_t))
{
    int64_t total = 0;
    for (int64_t i = 0; i < n; i++)
        total += f(i);
    return total;
}
```

## Mecanismo de paso

En LLVM, tanto la función E++ como el puntero C se
representan como `ptr` (opaque pointers). La función E++ se
convierte directamente al puntero sin thunk ni wrapper,
siempre que la firma sea compatible.
