# Interfaz de línea de comandos

Forma general:

```text
rivo archivo.ro [opciones]
```

```sh
./build/rivo hola.ro -o hola
./hola
```

Para listar todas las opciones ejecuta `rivo --help`; la implementación actual
incluye `-o`, `-I`, `-l`, `-L`, `-c`, `-S`, `-emit-ir`, `-g`, `-O0`, `-O1`, `-O2`, `-O3`,
`-v`/`--verbose`, `--keep-object`, `--run`, `--`, `--version`, `-h`/`--help`.
Véase [opciones](options.md).

## Manifiesto rivo.toml

Si existe un archivo `rivo.toml` subiendo desde el directorio del archivo de
entrada (o desde el directorio de trabajo), el compilador lo usa como
manifiesto de proyecto: puede declarar el archivo `entrada`, directorios de
dependencias, nombre del binario, perfil de build y opciones por defecto
(`-O`, `-g`; `advertencias_como_errores` convierte los avisos en errores). Si el manifiesto
declara `entrada`, basta con ejecutar `rivo` sin argumentos dentro del
proyecto.

En proyectos con manifiesto los artefactos van a `target/<perfil>/`, la
salida muestra el estado estilo Cargo con colores y existe una caché por
fecha que evita recompilar si nada cambió. Véase [manifiesto](manifest.md) y
[build](build.md).

## Gestor de paquetes

```sh
rivo install              # resuelve dependencias y escribe rivo.lock
rivo update               # vuelve a resolver y actualiza rivo.lock
rivo add nombre@^1.2.0    # anade una dependencia de version
rivo add ../libs/utils    # anade una dependencia de ruta
rivo add <url-git>#v1.0.0 # anade una dependencia git
rivo remove nombre        # elimina una dependencia
rivo tree                 # arbol de dependencias
rivo list                 # dependencias fijadas en rivo.lock
rivo search texto         # busca en el registro
rivo info nombre          # versiones publicadas
rivo publish v1.0.0       # publica en el registro git
```

Los subcomandos se reconocen como primer argumento y no interfieren con
`rivo archivo.ro`. Véase [gestor de paquetes](../modules/packages.md).

## `--run`

```sh
rivo archivo.ro --run              # compilar y ejecutar
rivo --run -- arg1 arg2            # argumentos para el programa
```

Compila y ejecuta el binario en una sola pasada, propagando su código de
salida. En proyectos con manifiesto se usa el binario de `target/`.
