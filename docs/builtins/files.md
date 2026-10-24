# Funciones incorporadas de archivos

`read_file(ruta)` lee un archivo; `write_file(ruta, contenido)` lo escribe y
devuelve `bool`; `file_exists(ruta)` comprueba su existencia.

```rivo
fn main():
    var ok:bool = write_file("salida.txt", "Rivo")
    if ok:
        print(read_file("salida.txt"))
```

Una lectura fallida puede producir error de runtime. Las rutas son relativas al
directorio de ejecución del programa. Para más operaciones, usa el módulo
[files](../stdlib/files.md).
