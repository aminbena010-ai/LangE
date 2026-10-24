# Variables con `var`

Declara una variable local con nombre, tipo y valor inicial:

```rivo
fn main():
    var contador:int = 0
    contador = contador + 1
    print(contador)
```

El tipo se exige explícitamente para variables ordinarias y no cambia durante
la vida de la variable. Para asignar una función a una variable, se puede
omitir el tipo; Rivo infiere su firma y permite llamar la variable:

```rivo
fn suma(a:int, b:int) -> int:
    return a + b

fn main():
    var operacion = suma
    print(operacion(2, 3))
```

Las declaraciones se usan dentro de funciones; los nombres que empiezan por
`_` son privados al exportar desde módulos. Ver [asignación](assignment.md).
