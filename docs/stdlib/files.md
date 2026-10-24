# Módulo estándar `files`

`files` expone operaciones sobre archivos y directorios:

```rivo
import files

fn main():
    var ok:bool = files.write("nota.txt", "Hola")
    if ok:
        print(files.read("nota.txt"))
```

API: `file_size`, `file_exists`, `delete_file`, `create_directory`,
`file_rename`, `read`, `write`, `exists`, `read_lines`, `write_lines`.
Comprueba el booleano de las operaciones que lo devuelven y los permisos del
proceso.
