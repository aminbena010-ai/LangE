# `string`

`string` representa texto entre comillas dobles. Se puede concatenar con `+` y
comparar por igualdad. Para interpolar expresiones simples usa un f-string.

```rivo
fn main():
    var nombre:string = "Ada"
    print("Hola " + nombre)
    print(f"Hola {nombre}")
```

Los escapes disponibles están en [escapes](../strings/escapes.md). El runtime y
varias funciones de texto cuentan bytes; no supongas índices Unicode.
