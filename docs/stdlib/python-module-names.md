# Módulos estándar con nombres de Python

Rivo está incorporando módulos cuyos nombres coinciden con los de la biblioteca
estándar de Python. La coincidencia de nombre no implica compatibilidad de API
ni de comportamiento: cada módulo ofrece las funciones que Rivo puede
implementar actualmente, con sus propios tipos y limitaciones.

## Módulos disponibles

Estos módulos ya existen como archivos `.ro` y se pueden importar:

| Módulo | Alcance disponible |
| --- | --- |
| [`string`](../../lib/string.ro) | Constantes ASCII, `title_words` y `swap_case`. |
| [`os`](../../lib/os.ro) | Directorio de trabajo, proceso, entorno y operaciones básicas de archivos. |
| [`pathlib`](../../lib/pathlib.ro) | Manipulación de rutas como texto; `Path` devuelve una cadena, no un objeto. |
| [`datetime`](../../lib/datetime.ro) | Fechas ISO `AAAA-MM-DD`, conversión de fecha Unix y operaciones de calendario. |
| [`platform`](../../lib/platform.ro) | Sistema operativo, hostname, arquitectura, procesadores y usuario. |
| [`base64`](../../lib/base64.ro) | Codificación y decodificación Base64 mediante `codecs`. |
| [`calendar`](../../lib/calendar.ro) | Año bisiesto, longitud y nombre del mes, y día de la semana. |
| [`fnmatch`](../../lib/fnmatch.ro) | Coincidencia de nombres con los comodines `*` y `?`. |
| [`html`](../../lib/html.ro) | Escape y unescape de un conjunto básico de entidades HTML. |
| [`shutil`](../../lib/shutil.ro) | Copia de texto y operaciones sencillas con rutas y archivos. |

También existen módulos con este nombre que ya tienen documentación propia:
[`math`](math.md), [`random`](random.md), [`statistics`](statistics.md),
[`json`](json.md), [`csv`](csv.md), [`textwrap`](textwrap.md), [`re`](re.md),
[`difflib`](difflib.md) y [`codecs`](codecs.md).

## Ejemplo

```rivo
import pathlib
import datetime
import fnmatch

fn main():
    print(pathlib.Path("datos/../salida.txt"))
    print(datetime.shift_date("2026-10-14", 1))
    print(fnmatch.matches("informe.csv", "*.csv"))
```

## API de los módulos añadidos

### `string`

- `digits()`, `ascii_lowercase()`, `ascii_uppercase()`, `ascii_letters()`,
  `hexdigits()`, `punctuation()` y `whitespace()` devuelven constantes ASCII.
- `title_words(texto)` pone en mayúscula la primera letra de cada palabra
  separada por espacios.
- `swap_case(texto)` invierte mayúsculas y minúsculas ASCII.

No implementa todavía todas las constantes, clases de caracteres Unicode ni la
API de cadenas de Python. Para operaciones generales de texto, consulta
[`text`](text.md).

### `os`

- `working_directory()` y `process_id()` consultan el directorio actual y el
  identificador del proceso.
- `environment_value(nombre)`, `environment_is_set(nombre)` y
  `update_environment(nombre, valor)` consultan o modifican el entorno.
- `path_exists(ruta)`, `path_size(ruta)`, `delete_path(ruta)`,
  `rename_path(origen, destino)` y `make_directory(ruta)` operan sobre archivos
  y directorios.

Este es un subconjunto: no incluye ejecución de procesos ni toda la API
multiplataforma de Python. Varias operaciones se apoyan en los módulos internos
`system` y `files`, cuyo soporte nativo puede depender de la plataforma.

### `pathlib`

- `Path(ruta)` normaliza la ruta y devuelve un `string`.
- `join_path(first, second)`, `file_name(ruta)` y
  `parent_directory(ruta)` componen o separan rutas.
- `extension_of(ruta)`, `file_stem(ruta)` y `extensions_of(ruta)` consultan la
  extensión, el nombre sin extensión y la lista de extensiones.
- `path_is_absolute(ruta)` y `path_parts(ruta)` consultan si la ruta es
  absoluta y sus componentes.

Rivo todavía no tiene clases; `Path` no crea un objeto con métodos como
`exists()` o `read_text()`. Las reglas de rutas son las que implementa
[`paths`](paths.md).

### `datetime` y `calendar`

`datetime.now()` devuelve la fecha y hora local; `today()` devuelve la fecha
local. `from_timestamp(segundos)` devuelve una fecha y `timestamp(fecha)`
convierte una fecha en segundos Unix. Las demás operaciones son
`shift_date(fecha, dias)`, `days_between_dates(inicio, fin)`,
`valid_date(fecha)`, `leap_year(anio)` y `weekday(fecha)`.

Las fechas se expresan como texto ISO `AAAA-MM-DD`. La conversión
`from_timestamp` devuelve solo la fecha, no una hora completa. La aritmética de
calendario y los detalles de zona horaria siguen las limitaciones de
[`date`](date.md).

`calendar` ofrece `leap_year(year)`, `month_length(year, month)`,
`month_label(month)`, `weekday_index(iso_date)` y
`weekday_label(iso_date)`. El índice del día usa la misma convención que
`date.day_of_week`.

### `platform`

`system_name()`, `node()`, `machine()`, `processor_count()` y `user_name()`
devuelven, respectivamente, el sistema operativo, hostname, arquitectura,
número de procesadores disponibles y nombre de usuario. Los resultados reflejan
la información que el sistema y el runtime pueden proporcionar.

### `base64`

`encode(texto)` codifica una cadena como Base64 y `decode(texto)` decodifica
Base64 a una cadena. Reutiliza [`codecs`](codecs.md) y trabaja con las cadenas
de bytes que maneja Rivo; no es una API de flujos de bytes arbitrarios.

### `fnmatch`

`matches(nombre, patron)` devuelve `true` si el nombre coincide con un patrón
que usa `*` (cero o más caracteres) y `?` (un carácter). No admite clases de
caracteres como `[abc]`, escapes ni reglas dependientes del sistema operativo.

### `html`

`escape(texto)` escapa `&`, `<`, `>`, comillas dobles y apóstrofos.
`unescape(texto)` reconoce las entidades producidas por `escape` y `&#39;`.
No es un parser HTML ni procesa todas las entidades con nombre o numéricas.

### `shutil`

- `copy_text(source, destination)` copia el contenido textual.
- `move_path(source, destination)`, `delete_path(path)` y
  `make_directory(path)` mueven, eliminan o crean rutas.
- `path_size(path)` consulta el tamaño.

`copy_text` no conserva metadatos ni permisos y no es una copia binaria general.
Las operaciones de lectura y escritura heredan el comportamiento y los errores
de [`files`](files.md).

## Estado del catálogo

El catálogo de 100 nombres en [`lib/c/copilot.txt`](../../lib/c/copilot.txt) es
una hoja de ruta, no una lista de módulos ya implementados. Solo deben
considerarse disponibles los módulos que tengan código y pruebas. Algunas APIs
de esta primera tanda reutilizan bibliotecas preexistentes y soporte del
runtime; todavía no se han migrado todas las operaciones a implementaciones
escritas enteramente en Rivo.
