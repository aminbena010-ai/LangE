# E++ — documentación oficial

E++ es un lenguaje de programación de propósito general diseñado en español. Tiene tipos explícitos, una sintaxis inspirada en Python y bloques definidos por indentación. Su compilador está escrito en C y genera código mediante LLVM.

> **Estado del proyecto:** E++ sigue en construcción. Esta guía describe el diseño aprobado y explica las limitaciones conocidas; que una sintaxis esté documentada no garantiza que todas sus combinaciones estén implementadas. La versión de referencia es **v0.8.0+08a2**: la suite end-to-end pasa 177/177 pruebas y la suite C del runtime pasa con AddressSanitizer y UndefinedBehaviorSanitizer. `try/catch`, `async`/`await` (fibras `ucontext` con planificador FIFO cooperativo), FFI con C y el gestor de paquetes están implementados. No se deben interpretar esos resultados como una certificación total del lenguaje.

## Índice

- [1. Empezar](#1-empezar)
- [2. Tu primer programa](#2-tu-primer-programa)
- [3. Sintaxis e indentación](#3-sintaxis-e-indentación)
- [4. Variables, constantes y tipos](#4-variables-constantes-y-tipos)
- [5. Operadores y expresiones](#5-operadores-y-expresiones)
- [6. Funciones](#6-funciones)
- [7. Control de flujo](#7-control-de-flujo)
- [8. Cadenas y f-strings](#8-cadenas-y-f-strings)
- [9. Listas y mapas](#9-listas-y-mapas)
- [10. Imports y módulos](#10-imports-y-módulos)
- [11. Funciones incorporadas](#11-funciones-incorporadas)
- [12. Biblioteca estándar](#12-biblioteca-estándar)
- [13. Funciones externas en C](#13-funciones-externas-en-c)
- [14. Errores y diagnósticos](#14-errores-y-diagnósticos)
- [15. Compilador y opciones de la CLI](#15-compilador-y-opciones-de-la-cli)
- [16. Arquitectura del compilador](#16-arquitectura-del-compilador)
- [17. Desarrollo y pruebas](#17-desarrollo-y-pruebas)
- [18. Referencia rápida](#18-referencia-rápida)

## 1. Empezar

### Requisitos

Para compilar E++ desde el código fuente se necesita:

- Un compilador de C compatible con C11, como GCC o Clang.
- CMake 3.16 o posterior.
- LLVM con sus archivos de desarrollo.
- `clang`, que el compilador usa para enlazar los programas generados.

No se necesitan dependencias de scripting para la documentación: el sitio `web/` es estático.

### Compilar

Desde la raíz del proyecto:

```sh
cmake -S . -B build
cmake --build build
./build/rivo --help
```

Para comprobar la versión:

```sh
./build/rivo --version
```

### Documentación web

El sitio del proyecto vive en `web/` y es 100% estático (HTML + CSS + JavaScript vanilla, sin build):

```sh
python3 -m http.server 8000 --directory web
```

Abre <http://127.0.0.1:8000> (o abre `web/index.html` directamente). Incluye la presentación del lenguaje, esta documentación en HTML y las guías *Python → Rivo* y *Godot → Rivo*. El sitio no compila ni ejecuta código Rivo.

## 2. Tu primer programa

Crea `hola.ro`:

```rivo
fn main():
    print("Hola, mundo")
```

Compílalo y ejecútalo:

```sh
./build/rivo hola.ro
./hola
```

`fn main()` es el punto de entrada. El cuerpo de la función está indentado; no se escriben llaves. También puedes especificar el archivo ejecutable de salida:

```sh
./build/rivo hola.ro -o saludo
./saludo
```

Un programa un poco más completo:

```rivo
fn doble(numero:int) -> int:
    return numero * 2

fn main():
    var valor:int = 21
    print(doble(valor))
```

El tipo de cada variable y parámetro se escribe explícitamente. E++ no infiere el tipo de una declaración.

## 3. Sintaxis e indentación

### Bloques

Los bloques empiezan después de `:` y se delimitan con indentación:

```rivo
if edad >= 18:
    print("Adulto")
else:
    print("Menor")
```

Usa `elif` para encadenar condiciones y `else` para el caso final (opcional):

```rivo
if edad < 13:
    print("Niño")
elif edad < 18:
    print("Adolescente")
else:
    print("Adulto")
```

`elif` es azúcar sintáctico: equivale a anidar un `if` dentro del bloque `else`. No existe `elseif`. No se usan `{}` para bloques. Mantén la misma indentación dentro de cada bloque. Una línea nueva termina una sentencia; las expresiones y llamadas no continúan en la línea siguiente.

`pass` es una sentencia vacía reservada, útil para dejar provisionalmente un bloque sin acciones:

```rivo
fn pendiente():
    pass
```

### Comentarios

Un comentario de una línea empieza por `#`:

```rivo
fn main():
    # Calcular el total
    var total:int = 12
```

Un comentario de varias líneas empieza con `#!` y termina en el primer `!#`:

```rivo
#!
Este bloque es un comentario.
No se anida.
!#
```

Un comentario de varias líneas sin cierre es un error. `//` no inicia un comentario.

### Identificadores públicos y privados

Un identificador que empieza por `_` es privado. Los demás son públicos. La regla se aplica a funciones, variables, constantes y parámetros; los módulos solo exponen funciones públicas al importador.

## 4. Variables, constantes y tipos

### Declaraciones

La forma de una variable es `var nombre:tipo = valor`; una constante usa `const`:

```rivo
fn main():
    var edad:int = 20
    edad = 21

    const PI:float = 3.14159
```

Las variables pueden reasignarse y las constantes no. No se realiza conversión implícita al asignar: un valor `int` no se convierte automáticamente a `float`.

**Ámbito global:** el diseño actual del proyecto establece que las declaraciones `var` y `const` se hacen dentro de funciones. Una prueba negativa (`variable_global`) comprueba que el compilador rechaza este caso con un diagnóstico; no declares `var`/`const` a nivel de módulo.

### Tipos oficiales

| Tipo | Uso |
| --- | --- |
| `int` | Número entero. |
| `float` | Número con parte fraccionaria. |
| `string` | Texto; se concatena con `+`. |
| `bool` | Valor `true` o `false`. |
| `char` | Un carácter, escrito entre comillas simples, por ejemplo `'A'`. |
| `list<T>` | Lista cuyos elementos tienen el tipo `T`, por ejemplo `list<int>`. |
| `map<K, V>` | Mapa de claves `K` a valores `V`, por ejemplo `map<string, int>`. |
| `struct` | Tipo de datos compuesto declarado por el programa. |
| `null` | Valor y tipo especial del lenguaje. |

No forman parte del lenguaje los tipos `void` o `any`. Una función sin tipo de retorno explícito no devuelve un valor; eso no añade un tipo `void` al lenguaje.

Las reglas de compatibilidad de `null` no deben suponerse a partir de otros lenguajes.

## 5. Operadores y expresiones

### Precedencia

De menor a mayor precedencia:

1. Asignación: `=`, `+=`, `-=`, `*=`, `/=`.
2. `or`.
3. `and`.
4. Comparaciones: `==`, `!=`, `>`, `<`, `>=`, `<=`.
5. Suma y resta: `+`, `-`.
6. Multiplicación, división y resto: `*`, `/`, `%`.
7. Operadores unarios: `-valor`, `not`.

Por ejemplo, `a + b * c` equivale a `a + (b * c)`.

### Aritmética

`+`, `-`, `*`, `/` y `%` son los operadores aritméticos. La división entre dos enteros produce un entero truncado hacia cero. Si al menos un operando es `float`, el resultado es `float`:

```rivo
fn main():
    var mitad:int = 7 / 2
    var mitad_float:float = 7.0 / 2
```

`var invalido:float = 7 / 2` es un error de tipo: no hay conversión implícita de `int` a `float`.

La aritmética entera se comprueba en runtime (arreglo 2026-10-16): si `+`, `-`, `*` o `/` sale del rango de `int`, el programa aborta con `OverflowError`; si el divisor de `/` o `%` es 0, aborta con `ZeroDivisionError`. Con un `try` activo, el error se captura en el `catch`. Antes de este arreglo los enteros desbordaban en silencio (por ejemplo `N = N * N` envolvía a 0).

Los `float` son `double` IEEE 754 con aritmética nativa del hardware (revisión 2026-10-16). Dividir o tomar el resto por `0.0` produce `inf` o `NaN` (no lanza error); `print` y `str` muestran la representación corta round-trip estilo `repr` de Python, con un decimal mínimo (`3.0`, `0.1 + 0.2` → `0.30000000000000004`).

La suma de dos listas del mismo tipo de elemento crea una lista nueva y no modifica las listas originales. `+=` asigna el resultado a la variable de la izquierda. Los mapas no admiten `+`.

### Comparaciones

`==` y `!=` comparan el contenido de cadenas. No se admiten comparaciones de orden (`<`, `>`, `<=`, `>=`) entre cadenas ni igualdad/desigualdad entre listas o mapas.

### Asignación y comparación

`=` asigna un valor; `==` compara dos valores:

```rivo
fn main():
    var puntos:int = 3
    puntos = 4

    if puntos == 4:
        print("Cuatro")
```

## 6. Funciones

Las funciones se declaran con `fn`, parámetros tipados y un tipo de retorno opcional:

```rivo
fn sumar(a:int, b:int) -> int:
    return a + b

fn saludar(nombre:string):
    print("Hola " + nombre)
```

`return` con un valor debe coincidir con el tipo de retorno declarado. El análisis semántico comprueba el uso del retorno; no des por hecho que todas las rutas de retorno están cubiertas por la implementación.

Una función cuyo nombre comienza por `_` es privada:

```rivo
fn _auxiliar(valor:int) -> int:
    return valor + 1
```

`main` solo puede declararse en el archivo principal, no en un módulo importado. Los programas correctos terminan con código de salida cero, salvo que fallen durante la ejecución.

## 7. Control de flujo

### Condicionales

```rivo
if temperatura > 30:
    print("Hace calor")
else:
    print("Temperatura moderada")
```

Las condiciones usan `bool`; combina condiciones con `and`, `or` y `not`.

### `while`

```rivo
fn main():
    var contador:int = 0
    while contador < 3:
        print(contador)
        contador += 1
```

### `for`

La forma diseñada es `for variable in iterable:`:

```rivo
for nombre in nombres:
    print(nombre)
```

Los recorridos por listas y mapas tienen cobertura en las pruebas existentes, pero no se debe inferir soporte para cualquier combinación de tipo o expresión iterable.

### `break` y `continue`

Ambos solo son válidos dentro de un bucle:

- `break` termina el bucle.
- `continue` pasa a la siguiente iteración.

### `try` y `catch`

La sintaxis es `try:` seguido de exactamente un `catch nombre:`. El bloque `catch` declara `nombre` como `string` con el mensaje del fallo. Se capturan los aborts de runtime (índice fuera de rango, clave inexistente, fichero inexistente, división por cero, desborde entero, memoria, etc.); fuera de un `try` siguen terminando el programa con código 1. `return`, `break` y `continue` están prohibidos dentro de un `try` (error semántico), pero se permiten dentro del `catch`. No existe `throw`: el lenguaje no lanza errores a mano.

```rivo
fn main():
    try:
        var lista:list<int> = [1, 2]
        print(lista[10])
    catch error:
        print(error)
```

El mecanismo usa setjmp/longjmp en el runtime (`RIVO_TRY_MAX_DEPTH = 64`).

### Concurrencia: `async`, `await`, `task<T>`

La concurrencia cooperativa usa `async fn`, el prefijo `await` y el tipo `task<T>`. Llamar a una `async fn` produce `task<T>`, `await` exige una `task<T>` y `async fn main()` es un error. La implementación usa fibras `ucontext` y un planificador FIFO cooperativo; `await` cede el control hasta que la tarea termina. En Windows las fibras no están soportadas y las pruebas correspondientes se omiten. Véase [docs/language/async.md](../docs/language/async.md).

## 8. Cadenas y f-strings

### Cadenas normales

Las cadenas se escriben con comillas dobles y admiten estos escapes:

| Escape | Significado |
| --- | --- |
| `\n` | Nueva línea |
| `\r` | Retorno de carro |
| `\t` | Tabulador |
| `\0` | Carácter nulo |
| `\\` | Barra invertida |
| `\"` | Comilla doble |
| `\'` | Comilla simple |

Los escapes desconocidos se conservan como barra invertida y carácter.

La concatenación de texto se puede hacer con `+` o con la función incorporada `concat(a, b)`. Las cadenas viven en memoria gestionada por el runtime; el programa E++ no las libera manualmente.

### F-strings

Un prefijo `f` permite interpolar valores dentro de `{}`:

```rivo
fn main():
    var nombre:string = "Ana"
    var edad:int = 20
    print(f"Hola {nombre}, tienes {edad} años")
```

`{{` y `}}` producen llaves literales. Dentro de las llaves se admite una expresión primaria con sufijos (por ejemplo, una llamada, un miembro o un índice), pero no operadores unarios ni binarios:

```rivo
print(f"Resultado: {sumar(2, 3)}")
# f"{a + b}" no está admitido
```

Solo se interpolan `int`, `float`, `string`, `bool` y `char`. Interpolar `list<T>`, `map<K, V>` o `null` genera un error semántico. El resultado de un f-string siempre es `string`.

## 9. Listas y mapas

### Listas

Una lista usa `list<T>` y corchetes:

```rivo
fn main():
    var numeros:list<int> = [1, 2, 3]
    var primero:int = numeros[0]
    numeros[0] = 10
    numeros.add(4)
```

El diseño también contempla `lista.remove(valor)`, `lista.len()`, `len(lista)` e indexación. La lectura o escritura de un índice fuera de rango termina el programa con un error de runtime. Las capacidades de listas están cubiertas por pruebas, pero son una parte activa del compilador/runtime: valida las operaciones con la versión instalada.

La concatenación `lista + otra_lista` crea una lista nueva y exige que ambas listas tengan el mismo tipo de elemento.

### Mapas

Los mapas usan `map<K, V>`, pares clave-valor entre corchetes e índices:

```rivo
fn main():
    var edades:map<string, int> = [
        "Ana": 20,
        "Luis": 25
    ]

    edades["Ana"] = 21
    edades["Pedro"] = 30
```

No se usan llaves para escribir mapas. El diseño incluye lectura, asignación, longitud e iteración. Consultar una clave inexistente es un error de runtime y termina el programa. Los mapas no admiten `+` ni comparación por `==`/`!=`.

### Structs

Un `struct` declara un tipo compuesto sin clases, métodos ni herencia. Cada
campo lleva un tipo y todos los campos se inicializan, en orden, al construir
un valor:

```rivo
struct Punto:
    var x:float
    var y:float

fn main():
    var punto:Punto = Punto(1.5, 2.5)
    punto.x = 3.0
    print(punto)
```

El constructor usa la forma `Nombre(valor1, valor2, ...)`; no hay valores por
defecto ni argumentos con nombre. El número y los tipos de argumentos deben
coincidir con los campos. `objeto.campo` permite leer y reasignar campos. Los
valores son referencias administradas por el runtime: asignarlos o incluirlos
en una lista conserva la identidad del objeto; `==` y `!=` comparan identidad,
no el contenido. `print` muestra `Punto(x=3.0, y=2.5)` y admite campos escalares
o structs anidados. Los campos `list`, `map` y `null` no se pueden imprimir
como parte de un struct. Los tipos struct no admiten `null` ni definiciones
recursivas. Los tipos struct definidos en módulos no se pueden importar como
tipos en el archivo principal.

## 10. Imports y módulos

Un módulo es un archivo `.ro` que se compila dentro de la misma unidad de compilación:

```rivo
import math
import math as m
from math import abs
```

- `from modulo import funcion` permite llamar `funcion(...)`.
- `import modulo as alias` permite llamar `alias.funcion(...)`.
- Sin alias, el nombre del módulo sirve como calificador.

Los imports solo se permiten a nivel global. Solo se pueden usar funciones públicas; los nombres que empiezan por `_` son privados. Los módulos no pueden definir `main`. Los imports circulares, los nombres duplicados y las colisiones en el espacio de nombres plano son errores.

### Búsqueda de módulos

Para un módulo `math`, el compilador busca `math.ro` en este orden:

1. El directorio del archivo principal.
2. Los directorios indicados con `-I`, en el orden de la CLI.
3. El directorio indicado por `RIVO_LIB`, si está definido.
4. La biblioteca estándar que acompaña al compilador.

Ejemplo:

```sh
./build/rivo programa.ro -I ./modulos
```

El directorio actual de la terminal no sustituye al directorio del archivo principal.

## 11. Funciones incorporadas

Las firmas de las funciones incorporadas son compartidas por el análisis semántico y el codegen.

### Consola y colecciones básicas

| Función | Firma | Descripción |
| --- | --- | --- |
| `print(valor)` | `int`, `float`, `string`, `bool` o `char` | Imprime el valor y una nueva línea. |
| `input()` | `() -> string` | Lee una línea de la entrada estándar; al llegar al fin de archivo devuelve `""`. |
| `len(valor)` | Texto, lista o mapa | Devuelve la longitud. En texto cuenta bytes, no necesariamente caracteres Unicode. |
| `concat(a, b)` | `(string, string) -> string` | Concatena dos cadenas. |

### Aleatoriedad y tiempo

| Función | Firma | Descripción |
| --- | --- | --- |
| `random(min, max)` | `(int, int) -> int` | Entero entre ambos límites, incluidos. `min > max` provoca error de runtime. |
| `random_float()` | `() -> float` | Float en `[0.0, 1.0)`. |
| `seed(valor)` | `(int)` | Fija la semilla para resultados reproducibles. |
| `time_ms()` | `() -> int` | Milisegundos monótonos desde la primera llamada. |
| `epoch_ms()` | `() -> int` | Milisegundos desde la época Unix. |

### Texto

| Función | Firma | Descripción |
| --- | --- | --- |
| `split(texto, separador)` | `(string, string) -> list<string>` | Divide el texto. Separador vacío: un carácter por elemento. |
| `join(lista, separador)` | `(list<string>, string) -> string` | Une elementos de texto. |
| `upper(texto)` | `(string) -> string` | Convierte a mayúsculas. |
| `lower(texto)` | `(string) -> string` | Convierte a minúsculas. |
| `trim(texto)` | `(string) -> string` | Recorta espacios de los extremos. |
| `substring(texto, inicio, longitud)` | `(string, int, int) -> string` | Extrae por índices de bytes; rango inválido o longitud negativa provoca error de runtime. |
| `replace(texto, viejo, nuevo)` | `(string, string, string) -> string` | Sustituye apariciones. |
| `contains(texto, parte)` | `(string, string) -> bool` | Indica si contiene el fragmento. |
| `starts_with(texto, prefijo)` | `(string, string) -> bool` | Comprueba el prefijo. |
| `ends_with(texto, sufijo)` | `(string, string) -> bool` | Comprueba el sufijo. |
| `find(texto, parte)` | `(string, string) -> int` | Devuelve la posición o `-1`. |

La clasificación y los índices del runtime de texto se basan en bytes y ASCII; no se debe asumir procesamiento Unicode completo.

### Conversión y numéricos

| Función | Firma | Descripción |
| --- | --- | --- |
| `str(valor)` | `int`, `float`, `bool`, `char` o `string` | Convierte el valor a `string`. |
| `ord(texto)` | `(string) -> int` | Código del primer carácter; `""` provoca error de runtime. |
| `chr(codigo)` | `(int) -> string` | Carácter del código como texto de un byte; el rango válido es 1–255 (0 no puede representarse en un C-string). |
| `to_int(texto)` | `(string) -> int` | Analiza un entero; el texto no válido provoca `ValueError` de runtime. |
| `to_float(texto)` | `(string) -> float` | Analiza un float; el texto no válido provoca `ValueError` de runtime. |
| `bin(valor)` | `(int) -> string` | Representación binaria con prefijo `0b`. |
| `oct(valor)` | `(int) -> string` | Representación octal con prefijo `0o`. |
| `hex(valor)` | `(int) -> string` | Representación hexadecimal con prefijo `0x` (minúsculas). |
| `round(valor)` | `(float) -> int` | Redondea al entero más cercano (la mitad se aleja de cero). |
| `pow(base, exponente)` | `(int, int) -> int` | Potencia entera; un exponente negativo provoca `ValueError` de runtime. |
| `abs(valor)` | `(int) -> int` | Valor absoluto. |

### Listas

| Función | Firma | Descripción |
| --- | --- | --- |
| `range(inicio, fin)` | `(int, int) -> list<int>` | Enteros desde `inicio` hasta `fin - 1`. |
| `divmod(a, b)` | `(int, int) -> list<int>` | `[a / b, a % b]` con división entera; `b == 0` provoca `ZeroDivisionError` de runtime. |
| `sorted(lista)` | `(list<int>) -> list<int>` | Copia ordenada de forma ascendente. |
| `reversed(lista)` | `(list<T>) -> list<T>` | Copia con el orden invertido. |
| `all(lista)` | `(list<bool>) -> bool` | `true` si todos los elementos son ciertos; una lista vacía devuelve `true`. |
| `any(lista)` | `(list<bool>) -> bool` | `true` si algún elemento es cierto; una lista vacía devuelve `false`. |

### Archivos

| Función | Firma | Descripción |
| --- | --- | --- |
| `read_file(ruta)` | `(string) -> string` | Lee el archivo; si no existe, provoca error de runtime. |
| `write_file(ruta, contenido)` | `(string, string) -> bool` | Escribe el contenido e indica éxito. |
| `file_exists(ruta)` | `(string) -> bool` | Comprueba la apertura del archivo; los directorios devuelven `false`. |

### Consola y espera

| Función | Firma | Descripción |
| --- | --- | --- |
| `clear()` | `()` | Escribe la secuencia ANSI para borrar la pantalla. |
| `move(fila, columna)` | `(int, int)` | Mueve el cursor mediante ANSI; las coordenadas empiezan en 1. |
| `sleep_ms(milisegundos)` | `(int)` | Espera; un valor negativo no espera. |
| `read_key()` | `() -> char` | Lee una tecla sin Intro; EOF devuelve el carácter 0. |

Las cadenas devueltas por el runtime pertenecen a su arena y no se liberan manualmente. Los códigos ANSI también se verán si la salida se redirige a un archivo.

## 12. Biblioteca estándar

Los módulos de la biblioteca estándar se importan como cualquier módulo. Sus fuentes E++ están en `lib/` (extensión `.ro`); algunos usan archivos C bajo `lib/c/`. Las funciones que empiezan por `_` son auxiliares privados y no forman parte de la API pública. Los nombres de módulo y de funciones siguen el estilo de Python (migración anglicista de 2026-10-09/11).

| Módulo | API pública y propósito |
| --- | --- |
| `math` | `square_root`, `power`, `sine`, `cosine`, `tangent`, `ln`, `log_10`, `exponential`, `pi`, `euler`, `round_to_int`, `floor_to_int`, `ceil_to_int`, `is_integer`, `abs`, `abs_float`, `min`, `max`, `clamp`, `is_even`, `is_odd`, `sign`, `integer_power`, `factorial`, `gcd`, `lcm`, `average`, `celsius_to_fahrenheit`, `fahrenheit_to_celsius`. |
| `text` | `to_upper`, `to_lower`, `strip`, `parts`, `combine`, `substitute`, `includes`, `has_prefix`, `has_suffix`, `index_of`, `slice`, `is_empty`, `repeat`, `count`, `capitalize`, `reverse`, `collapse_spaces`. |
| `time` | `now_ms`, `unix_ms`, `ms_to_seconds`, `sleep`, `sleep_seconds`. |
| `random` | `set_seed`, `integer`, `uniform`, `chance`. |
| `files` | `read`, `write`, `exists`, `read_lines`, `write_lines`, `file_size`, `file_exists`, `delete_file`, `create_directory`, `file_rename`. |
| `json` | `encode`, `decode`, `escape`, `unescape`. JSON plano: mapas de `string` a `string`, sin objetos ni listas anidados. Un JSON inválido provoca error de runtime. |
| `date` | `current_date`, `current_time`, `current_datetime`, `epoch_to_date`, `date_to_epoch`, `add_days`, `days_between`, `is_leap_year`, `is_valid_date`, `day_of_week`, `day_of_week_name`, `days_in_month`, `month_name`. Calendario gregoriano; fechas `"AAAA-MM-DD"`, horas `"HH:MM:SS"`; conversiones de época en UTC, hora actual en local. |
| `system` | `pid`, `current_directory`, `hostname`, `os`, `architecture`, `get_env`, `has_env`, `set_env`, `username`, `cpu_count`, `total_memory_mb`, `available_memory_mb`, `uptime`. Memoria y `uptime` solo dan valores en Linux (en otro SO devuelven `-1`). |
| `statistics` | `sum`, `product`, `mean`, `min_list`, `max_list`, `range`, `median`, `mode`, `variance`, `standard_deviation`, `percentile`. Trabaja con `list<float>`, calcula varianza poblacional, no muta la entrada y devuelve 0 para lista vacía. |
| `validation` | `numeric`, `is_float`, `not_empty`, `min_length`, `max_length`, `in_range`, `letters`, `alphanumeric`, `is_email`. `is_float` requiere un punto: `"3.14"` sí, `"3"` no. |
| `csv` | `parse`, `format`, `row_to_string`, `quote`, `unquote`, `columns`. Usa comillas dobles, `""` para escapar una comilla y normaliza `\r\n`. |
| `paths` | `join`, `name`, `directory`, `extension`, `without_extension`, `name_without_extension`, `is_absolute`, `components`, `normalize`. Manipula texto, no el sistema de archivos; `normalize` devuelve `"."` si el resultado queda vacío. |
| `color` | `colorize`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `black`, `gray`, `bold`, `underline`, `invert`, `reset`, `color_256`, `color_256_fondo`, `color_rgb`, `color_rgb_fondo`, `no_color`. Secuencias ANSI; `color_256` y `color_rgb` aceptan colores de 256 y RGB verdadero. |
| `codecs` | `base64_encode`, `base64_decode`, `hex_encode`, `hex_decode`, `url_quote`, `url_unquote`, `rot13`. |
| `re` | `match`, `search`, `test`, `findall`, `sub`, `split`. Expresiones regulares. |
| `strings` | `digits`, `ascii_lowercase`, `ascii_uppercase`, `ascii_letters`, `hex_digits`, `punctuation`, `whitespace`, `lstrip`, `rstrip`, `swapcase`, `capwords`. Constantes y utilidades de cadenas estilo Python. |
| `string` | `digits`, `ascii_lowercase`, `ascii_uppercase`, `ascii_letters`, `hexdigits`, `punctuation`, `whitespace`, `title_words`, `swap_case`. |
| `textwrap` | `wrap`, `fill`, `indent`, `dedent`, `shorten`. Ajuste de texto a un ancho dado (codicioso). |
| `difflib` | `ratio`, `unified_diff`, `get_close_matches`. Comparación de cadenas y diff unificado. |
| `base64` | `encode`, `decode`. Codificación base64. |
| `html` | `escape`, `unescape`. Escape de entidades HTML. |
| `fnmatch` | `matches`. Coincidencia de patrones de nombre de fichero. |
| `os` | `working_directory`, `process_id`, `environment_value`, `environment_is_set`, `update_environment`, `path_exists`, `path_size`, `delete_path`, `rename_path`, `make_directory`. |
| `pathlib` | `Path`, `join_path`, `file_name`, `parent_directory`, `extension_of`, `file_stem`, `extensions_of`, `path_is_absolute`, `path_parts`. |
| `platform` | `system_name`, `node`, `machine`, `processor_count`, `user_name`. |
| `shutil` | `copy_text`, `move_path`, `delete_path`, `make_directory`, `path_size`. |
| `calendar` | `leap_year`, `month_length`, `month_label`, `weekday_index`, `weekday_label`. |
| `datetime` | `now`, `today`, `from_timestamp`, `timestamp`, `shift_date`, `days_between_dates`, `valid_date`, `leap_year`, `weekday`. |
| `mgl` | GUI minimalista estilo Tk (nombres en inglés): `available`, `window`, `title`, `geometry`, `resizable`, `label`, `button`, `entry`, `checkbutton`, `frame`, `config`, `get`, `insert`, `delete`, `update`, `wait`, `event_type`, `event_widget`, etc. Requiere un backend gráfico disponible. |

Ejemplo:

```rivo
import math

fn main():
    var resultado:float = math.square_root(81.0)
    print(resultado)
```

El espacio de nombres es plano: evita nombres que choquen entre tu programa y los módulos importados.

## 13. Funciones externas en C

`external` declara una función C sin cuerpo E++:

```rivo
external "c/calculos.c" fn sumar(a:int, b:int) -> int
```

La ruta relativa se resuelve desde el directorio del archivo `.ro` que contiene la declaración. El compilador compila el archivo C con `clang` y enlaza su objeto con el programa; no usa `dlopen`.

Los parámetros y retornos permitidos son `int`, `float`, `string`, `bool` y `char`. Una función sin `-> tipo` no devuelve valor. No se pueden pasar ni devolver `list<T>` o `map<K, V>`.

El archivo fuente C se compila una sola vez aunque haya varias declaraciones que lo referencien. Las opciones de include para su compilación incluyen el directorio del programa, los directorios `-I` y la biblioteca estándar. En Linux el enlace añade `-lm`.

**Vida de cadenas:** una cadena devuelta por código C debe ser estática o seguir siendo válida durante toda la ejecución; E++ no la copia. La declaración `external` es la fuente de verdad de la firma: E++ no declara cabeceras C por separado.

## 14. Errores y diagnósticos

El compilador distingue errores léxicos, sintácticos, semánticos, de generación de código e internos. Cada error se imprime bajo una clase estilo Python (`SyntaxError`, `TypeError`, `NameError`, `ImportError`, etc.) en stderr. Un error puede incluir ubicación, archivo, detalle y sugerencia; el nivel de detalle depende de la implementación actual.

Ejemplo del formato de diagnóstico esperado:

```text
NameError: funcion no encontrada
  --> programa.ro:2:1
   |
 2 | printt("x")
   | ^^^^^^
   |
   = sugerencia: quizas quisiste decir 'print'
```

Cuando stderr es una terminal (y no hay la variable `NO_COLOR`), la cabecera, el `-->`, los números de línea, el `^`, `detalle:` y `sugerencia:` se emiten con colores ANSI; en fichero o pipe, texto plano.

No todas las rutas producen todavía todos los campos del ejemplo. Las fallas durante la ejecución que el diseño define como fatales incluyen índices de lista fuera de rango, claves inexistentes, `random` con límites invertidos y errores como leer un archivo inexistente.

`try/catch` captura los errores de ejecución listados arriba cuando el código está dentro de un `try`. Fuera de un `try`, el runtime aborta con `<Clase>: <mensaje>` por stderr y código de salida 1. Las clases de error de runtime incluyen `IndexError`, `KeyError`, `FileNotFoundError`, `ValueError`, `ZeroDivisionError`, `OverflowError`, `MemoryError`, `TypeError`, `RuntimeError` y `SystemError`.

## 15. Compilador y opciones de la CLI

Forma general:

```text
rivo <archivo.ro> [opciones]
```

| Opción | Efecto |
| --- | --- |
| `-o archivo` | Nombre del ejecutable de salida. |
| `-I dir` o `-Idir` | Añade un directorio de búsqueda de módulos; puede repetirse. |
| `-l lib` o `-lib` | Biblioteca del sistema para el enlace final (p. ej. `-l m` enlaza `libm`); puede repetirse. |
| `-L dir` o `-Ldir` | Directorio extra donde buscar esas bibliotecas; puede repetirse. |
| `-c` | Genera un objeto sin enlazar. |
| `-S` | Genera ensamblador. |
| `-emit-ir` | Emite LLVM IR. |
| `-g` | Incluye información de depuración. |
| `-O0`, `-O1`, `-O2` | Selecciona nivel de optimización. |
| `-v`, `--verbose` | Muestra información del compilador. |
| `--keep-object` | Conserva el archivo objeto temporal. |
| `--run` | Compila y ejecuta el binario en una sola pasada; `--` separa los argumentos del programa. |
| `--version` | Muestra la versión. |
| `-h`, `--help` | Muestra la ayuda. |

Ejemplos:

```sh
./build/rivo programa.ro -o salida
./build/rivo programa.ro -emit-ir
./build/rivo programa.ro -g -O1
./build/rivo programa.ro -I ./modulos -I ./compartidos
./build/rivo programa.ro -L ./libs -l m -l pthread
./build/rivo --run programa.ro -- arg1 arg2
```

`-l` y `-L` se pasan al enlace final (clang), igual que en C: `-l m` enlaza
`libm` y `-L ./libs` añade un directorio de búsqueda. Para declarar funciones
C en E++ usa `external`; el flujo completo está en
[docs/ffi/build-and-link.md](../docs/ffi/build-and-link.md).

### Manifiesto `rivo.toml`

Un proyecto puede llevar un manifiesto opcional `rivo.toml` (estilo `Cargo.toml`) con tablas `[proyecto]` (nombre, versión, entrada, binario), `[dependencias]` (directorios de búsqueda de módulos, como `-I`) y `[compilador]` (optimizar, depurar, perfil, advertencias_como_errores; esta última convierte los avisos en errores). El compilador lo busca automáticamente subiendo desde el directorio del archivo de entrada (hasta 32 niveles). Con manifiesto, los artefactos van a `target/debug/` o `target/release/`, la compilación muestra un estado estilo Cargo y hay una caché por huella SHA-256 que evita recompilar si nada cambió. Los flags de la CLI tienen prioridad sobre el manifiesto. Véase [docs/compiler/manifest.md](../docs/compiler/manifest.md) y [docs/compiler/build.md](../docs/compiler/build.md).

## 16. Arquitectura del compilador

El pipeline del compilador es:

```text
código E++
    ↓
lexer
    ↓
parser
    ↓
AST
    ↓
análisis semántico
    ↓
LLVM IR
    ↓
archivo objeto
    ↓
clang + runtime
    ↓
ejecutable
```

El proyecto separa las responsabilidades principales:

| Directorio/archivo | Responsabilidad |
| --- | --- |
| `include/` | Interfaces del AST, lexer, parser, semántica, errores, LLVM, linker, runtime, tipos y tokens. |
| `src/lexer.c`, `src/token.c` | Reconocimiento y representación de tokens. |
| `src/parser.c`, `src/ast.c` | Análisis sintáctico y nodos del AST. |
| `src/semantic.c` | Scopes, nombres, tipos y comprobaciones semánticas. |
| `src/llvm.c` | Generación de LLVM IR. |
| `src/linker.c`, `src/main.c` | Enlace y coordinación del CLI/pipeline. |
| `src/manifest.c`, `src/build.c` | Manifiesto `rivo.toml`, perfiles y caché de `target/`. |
| `src/error.c`, `src/suggest.c` | Diagnósticos y sugerencias del compilador. |
| `src/runtime/` | Funciones de runtime para valores, listas, mapas, texto, ficheros, tiempo y consola. |
| `lib/`, `lib/c/` | Biblioteca estándar en E++ y sus complementos C. |
| `tests/` | Pruebas end-to-end, negativas, de biblioteca estándar y runtime. |
| `web/` | Sitio web estático del proyecto (documentación y guías). |

La presencia de una interfaz o una estructura en el código no demuestra por sí sola que una característica esté completa. La suite y los ejemplos ejecutables son la referencia práctica para el estado del checkout.

## 17. Desarrollo y pruebas

Desde la raíz del proyecto:

```sh
sh tests/run_tests.sh
sh tests/runtime/run_tests.sh
```

La primera suite compila y ejecuta programas E++, comprueba errores esperados y cubre módulos y biblioteca estándar. La segunda compila el runtime C con AddressSanitizer y UndefinedBehaviorSanitizer.

Para escribir una prueba:

- Añade programas que deben ejecutarse en `tests/integration/` o `tests/stdlib/`.
- Añade entradas que deben rechazarse en `tests/errors/`.
- Añade pruebas C del runtime bajo `tests/runtime/`.
- Registra las pruebas end-to-end en `tests/run_tests.sh` para que se ejecuten.

### Resultado de referencia de esta edición

El 24 de octubre de 2026, con el checkout de la versión 0.8.0+08a2:

- `./build/rivo --version` informó la versión `0.8.0+08a2`.
- `sh tests/run_tests.sh`: 177 pruebas pasaron y 0 fallaron.
- `sh tests/runtime/run_tests.sh`: pasó (runtime C con AddressSanitizer y UndefinedBehaviorSanitizer).

Estos resultados son una fotografía puntual, no una afirmación de que las pruebas siempre fallen así. Si la suite falla en tu entorno, conserva la salida completa y revisa primero el error concreto; no ocultes un fallo como si la prueba hubiera pasado.

## 18. Referencia rápida

```rivo
fn sumar(a:int, b:int) -> int:
    return a + b

fn main():
    const LIMITE:int = 3
    var contador:int = 0
    while contador < LIMITE:
        print(f"vuelta {contador}: {sumar(contador, 1)}")
        contador += 1
```

Reglas para recordar:

- Los bloques usan indentación y `:`; no usan llaves.
- Declara tipos explícitamente: `var nombre:tipo = valor`.
- Usa `==` para comparar y `=` para asignar.
- `if` admite `else` y `elif` encadenados; no existe `elseif`.
- Los bucles son `while` y `for`; `break` y `continue` requieren un bucle.
- Los mapas y listas usan corchetes; `{}` solo tiene un papel dentro de f-strings.
- No se convierten tipos implícitamente.
- E++ sigue en desarrollo: comprueba el soporte real con ejemplos y tests del checkout.

## Documentos relacionados

- [README del proyecto](../README.md): resumen y comandos iniciales.
- [Especificación y reglas del proyecto](./claude.md): decisiones de diseño y convenciones para contribuir.
- [Guías por tema](../docs/README.md): referencia organizada por concepto con ejemplos.
- [Sitio web](../web/index.html): presentación, documentación y guías (estático, sin servidor propio).
- [Registro de errores y decisiones](./error.log): historial de problemas y soluciones.
