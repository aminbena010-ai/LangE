# `char`

`char` representa un carácter de un byte, escrito entre comillas simples. Se
admiten los escapes documentados para strings, como `'\n'` y `'\''`.

```rivo
fn main():
    var inicial:char = 'R'
    print(inicial)
```

El soporte de caracteres no implica Unicode completo. `ord(string)` y
`chr(int)` son conversiones incorporadas; consulta [texto](../builtins/text.md).
