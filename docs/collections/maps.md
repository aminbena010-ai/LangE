# Mapas

Un mapa asocia claves con valores: `map<tipo_clave, tipo_valor>`. Los literales
usan pares `clave: valor` entre corchetes o llaves. Se pueden distribuir en
varias líneas; en ese caso, separa las entradas con comas.

```rivo
fn main():
    var edades:map<string, int> = {
        "Ada": 36,
        "Lin": 25,
    }
    edades["Lin"] = 25
    print(edades["Lin"])
    print(len(edades))
    edades.remove("Ada")
    print(len(edades))
```

La forma compacta existente también es válida: `["Ada": 36]`. Los mapas vacíos
pueden escribirse como `{}` o `[]` cuando el tipo se conoce por la anotación.
Los bloques de funciones y control siguen delimitándose con indentación, no con
llaves.

La lectura de una clave inexistente termina con error de runtime; una
asignación a clave nueva la inserta.

Un mapa tiene dos operaciones además del acceso por índice: `len(mapa)` y
`mapa.len()` (0.3.0) devuelven el número de entradas, y `mapa.remove(clave)`
elimina la entrada de esa clave. Si la clave no existe, `remove` no hace nada.
No hay más métodos de mapa.
