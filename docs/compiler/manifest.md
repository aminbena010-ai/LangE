# Manifiesto de proyecto (rivo.toml)

`rivo.toml` es un archivo opcional, al estilo de `Cargo.toml`, que describe un
proyecto escrito en E++. El compilador lo busca automáticamente subiendo desde
el directorio del archivo de entrada (o desde el directorio de trabajo si no se
pasó ninguno). Si no existe, todo funciona igual que sin manifiesto.

## Ejemplo

```toml
# rivo.toml

[proyecto]
nombre = "miapp"
version = "0.1.0"
entrada = "src/main.ro"
binario = "miapp"

[dependencias]
utilidades = "libs"

[compilador]
optimizar = "2"
depurar = "true"
perfil = "release"
advertencias_como_errores = "true"
```

Con este manifiesto, ejecutar `rivo` dentro del proyecto compila
`src/main.ro`, busca módulos importados también en `libs/` (sin necesidad de
`-I`), aplica `-O2 -g` como valores por defecto y guarda los artefactos en
`target/release/` (véase [build](build.md)).

## Formato

El parser es un subconjunto deliberadamente mínimo de TOML: cadenas entre
comillas dobles, cabeceras `[tabla]`, comentarios `#`, pares
`clave = "valor"` y, solo en `[dependencias]`, tablas en línea
`{ version = "..." }` / `{ ruta = "..." }`. No hay arrays.

### `[proyecto]`

| Clave     | Significado                                              |
| --------- | -------------------------------------------------------- |
| `nombre`  | Nombre del proyecto (informativo, y etiqueta del estado de compilación) |
| `version` | Versión del proyecto (informativa)                        |
| `entrada` | Ruta al `.ro` principal, relativa al manifiesto. Permite compilar sin escribir el archivo en la CLI |
| `binario` | Nombre del ejecutable generado en `target/<perfil>/`. Por defecto, `nombre`; sin manifiesto, el nombre del `.ro` |

### `[dependencias]`

Cada clave es el nombre de la dependencia. El valor puede ser:

```toml
[dependencias]
# Ruta local (compatible con versiones anteriores)
utils = "../libs/utils"
utilidades = "libs"

# Version semver: se busca en .rivo/deps/<nombre> o vendor/<nombre>
jsonlib = "1.2.0"                       # ^1.2.0
mgl = { version = ">=0.3, <0.4" }
# Ruta explicita
otra = { ruta = "../otra" }

# Repositorio Git fijado por tag
saludo = { git = "https://github.com/usuario/saludo", tag = "v1.0.0" }
```

- Una **ruta** se añade a la búsqueda de módulos `.ro`, igual que un `-I`.
- Una **versión** se resuelve con `rivo install` y se fija en `rivo.lock`.
  Sin registro remoto (hasta la v0.6), el paquete debe estar desempaquetado en
  `.rivo/deps/<nombre>/` o `vendor/<nombre>/`.
- `1.2.0` equivale a `^1.2.0`; también se admiten `~`, `=`, `>=`, `>`, `<=`,
  `<`, `*` y varias restricciones separadas por comas.
- Una dependencia **git** se clona en `.rivo/deps/<nombre>`; `tag` es
  obligatorio para que el build sea reproducible.

Véase [gestor de paquetes](../modules/packages.md).

### `[registro]`

```toml
[registro]
url = "https://github.com/usuario/rivo-packages"
```

URL del repositorio Git que hace de índice de paquetes. Es opcional: sin ella
solo se resuelven dependencias de ruta y paquetes ya presentes en
`.rivo/deps` o `vendor`. `rivo install` clona el índice en `.rivo/registro`.
Véase [gestor de paquetes](../modules/packages.md).

### `rivo.lock`

`rivo install` genera `rivo.lock` con la versión y el origen exactos de cada
dependencia (incluidas las transitivas); las dependencias Git y del registro
se materializan en `.rivo/deps/<nombre>`. El compilador lee ese archivo para
añadir las rutas resueltas a la búsqueda de módulos. Si una dependencia de
versión no está en el lock, la compilación pide ejecutar `rivo install`.

### `[compilador]`

| Clave                        | Valor                     | Efecto          |
| ---------------------------- | ------------------------- | --------------- |
| `optimizar`                  | `"0"`, `"1"` o `"2"`      | Nivel `-O`      |
| `depurar`                    | `"true"` o `"false"`      | Información `-g`|
| `perfil`                     | `"debug"` o `"release"`   | Carpeta de `target/` y optimización por defecto |
| `advertencias_como_errores`  | `"true"` o `"false"`      | Las advertencias detienen la compilación. Hoy no tiene efecto: el compilador no emite advertencias todavía |

Los detalles de selección de perfil y de la caché de `target/` están en
[build](build.md).

## Prioridad

Los flags de la línea de comandos siempre tienen prioridad sobre el
manifiesto: si el usuario pasa `-O1` o `-g`, el valor del `rivo.toml` se
ignora para esa opción. Las rutas de `[dependencias]` se suman a las dadas
con `-I` (sin duplicados).

## Errores

Un `rivo.toml` mal formado (clave fuera de tabla, tabla o clave desconocida,
valor sin comillas, clave duplicada) se rechaza con un mensaje que indica la
línea:

```text
rivo: rivo.toml, línea 3: clave fuera de una tabla (falta una cabecera como [proyecto])
```

Si `entrada` apunta a un archivo que no existe, la compilación también falla
antes de leer ninguna fuente.

## Búsqueda

Desde el directorio del archivo de entrada se sube componente a componente
(`../`) hasta 32 niveles buscando `rivo.toml`. El primero que se encuentre es
el que se usa. Si hay varios proyectos anidados, cada uno puede llevar el
suyo.
