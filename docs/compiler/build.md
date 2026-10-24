# Sistema de build (`target/`, perfiles, caché)

Cuando un proyecto lleva un manifiesto [`rivo.toml`](manifest.md), el
compilador adopta un estilo de build similar al de Cargo, con mejoras:

- los artefactos se guardan en `target/<perfil>/` dentro del proyecto;
- la salida muestra el estado de la compilación con colores, al estilo de
  Cargo;
- una **caché por huella** (fecha + SHA-256) evita recompilar si nada cambió
  de verdad (los rebuilds son instantáneos y un `touch` no fuerza rebuild);
- `--run` compila y ejecuta en una sola pasada;
- el modo `-v` muestra el tiempo de cada fase.

Sin `rivo.toml`, nada de esto aplica: el comportamiento es el mismo de
siempre, con el ejecutable junto al archivo de entrada y con el nombre de la
fuente sin la extensión `.ro` (`hola.ro` genera `hola`), salvo que `-o` fije
otra ruta. Si se pasa `-o`, la salida explícita manda y no se usa `target/`.

## Estructura de `target/`

```text
miapp/
  rivo.toml
  src/main.ro
  target/
    debug/
      miapp              # ejecutable
      miapp.o            # objeto intermedio (se conserva)
      miapp.fuentes      # caché: lista de fuentes del build
      miapp.huella       # caché: versión, flags y SHA-256 de cada fuente
    release/
      ...
```

## Perfiles

| Perfil | Carpeta | Optimización por defecto |
| --- | --- | --- |
| `debug` | `target/debug/` | `-O0` |
| `release` | `target/release/` | `-O2` |

La selección sigue esta prioridad:

1. Si la CLI dio `-O0`/`-O1`/`-O2`: la carpeta es `release` con `-O2` o más,
   `debug` en caso contrario.
2. Si no, y el manifiesto declara `perfil = "release"` (o `"debug"`): se usa
   esa carpeta y el nivel de optimización por defecto del perfil (si además
   hay `optimizar`, manda ese).
3. Si no, sigue a `optimizar` del manifiesto (`"2"` → `release`).
4. Por defecto: `debug`.

## Salida estilo Cargo

Solo en proyectos con manifiesto, por stderr (stdout queda limpia para
`--run`):

```text
   Compilando miapp v0.1.0 (/ruta/miapp)
    Enlazando debug
    Finished debug en 0.42s
```

Con color cuando stderr es una terminal y no está definida la variable
`NO_COLOR`; en pipes y ficheros de log sale texto plano, sin escapes ANSI.

```sh
rivo                      # compila con estado
rivo -v                   # además, etapas y tiempos por fase
NO_COLOR=1 rivo           # estado sin color
```

Ejemplo de `-v`:

```text
[rivo] usando manifiesto rivo.toml
[rivo] análisis: 0.8 ms
[rivo] imports: 0.2 ms
[rivo] semántica: 1.1 ms
[rivo] código: 12.4 ms
[rivo] objeto: 3.5 ms
```

## Caché por huella (fecha + SHA-256)

Tras cada build completo se guardan dos archivos en `target/<perfil>/`:

- `<binario>.fuentes`: lista de ficheros `.ro` (principal, imports y
  biblioteca estándar) y el `rivo.toml`;
- `<binario>.huella`: versión del compilador, nivel de optimización, perfil
  y **SHA-256 del contenido** de cada fuente, además del runtime enlazado.

En la siguiente ejecución hay dos caminos:

1. **Rápido**: si ninguna fuente es más nueva que `.huella` (con 1 s de
   margen por la resolución de `st_mtime`), se reutiliza el binario sin leer
   contenidos:
   ```text
       Finished debug (sin cambios) en 0.00s
   ```
2. **Lento**: si alguna fecha cambió, se recalculan los hashes. Si el
   contenido es idéntico (un `touch`), el build sigue considerándose fresco;
   si difiere, se recompila.

Se invalida la caché al cambiar cualquier `.ro`, el manifiesto, el nivel
`-O`, la versión del compilador o el runtime. La caché solo aplica a builds
completos (no con `-c`, `-S` ni `-emit-ir`) y solo cuando no se pasa `-o`.

## `--run`

```sh
rivo --run                 # compilar y ejecutar
rivo --run -- arg1 arg2    # pasar argumentos al programa
```

El código de salida del programa se propaga al del compilador.

## Interacción con las opciones de la CLI

| Situación | Resultado |
| --- | --- |
| `-o salida` | Ruta explícita; sin `target/` ni caché |
| `-c`, `-S`, `-emit-ir` | Artefacto junto a la ruta de salida; sin caché |
| `--keep-object` | El objeto en `target/` se conserva siempre |
| Sin manifiesto | Comportamiento histórico completo |

## Implementación

- `include/build.h` y `src/build.c`: rutas, directorios, caché, estado con
  color y tiempos.
- Integración en `src/main.c` (sección BUILD tras cargar el manifiesto).
