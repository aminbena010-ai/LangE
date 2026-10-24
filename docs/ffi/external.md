# Funciones externas en C

`external` declara una función implementada fuera de Rivo. El primer argumento
es el archivo fuente C que compilará el enlazador:

```rivo
external "helper.c" fn triple(n:int) -> int

fn main():
    print(triple(7))
```

`helper.c` debe exportar una función C compatible con la firma declarada. Las
rutas relativas se resuelven respecto al módulo que contiene la declaración.
El compilador compila el C con `clang` y enlaza el objeto con el programa;
consulta [compilación y enlace](build-and-link.md).
