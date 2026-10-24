# Verificación ABI (FFI)

La verificación ABI comprueba en compile-time que las firmas
`external` declaradas en `.ro` coinciden con las definiciones
en el fichero `.c`.

## Uso

```bash
rivo programa.ro --ffi-check
```

Sin `--ffi-check`, el compilador no verifica las firmas y
confía en que el enlace resuelva las diferencias.

## Cómo funciona

1. Por cada fichero `.c` externo, se genera un header
   `<salida>.rivo_ffi.h` con las declaraciones C esperadas
   de todas las funciones `external` apuntando a ese `.c`.
2. El header se fuerza con `-include` al compilar el `.c`.
3. Si el `.c` define una función con firma distinta, clang
   emite `conflicting types for 'nombre'` en compile-time.

## Ejemplo de error

```
tests/fixtures/abi_bad.c:8:5: error: conflicting types for 'suma'
    8 | int suma(int a, int b)
      |     ^
./programa.o.ext0.o.rivo_ffi.h:7:16: note: previous declaration is here
    7 | extern int64_t suma(int64_t, int64_t);
      |                ^
```

## Limitaciones

La verificación es estricta: C no considera `long long`,
`int64_t` y `long` como tipos compatibles (aunque todos
miden 64 bits en Linux x86-64). Si tu código C usa un tipo
diferente al del header generado, obtendrás un falso
positivo. En ese caso, omite `--ffi-check` o ajusta el
`.c` para usar los tipos stdint (`int64_t`, `uint32_t`, etc.).

## Desactivación

`--ffi-check` está desactivado por defecto. Actívalo solo
si necesitas verificar que las firmas coinciden exactamente.
