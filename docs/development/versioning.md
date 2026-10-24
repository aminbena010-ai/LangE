# Versionado

Rivo identifica las versiones publicadas con tres números:

```text
MAJOR.MINOR.PATCH
```

El proyecto está en desarrollo y todavía no ha llegado a `1.0.0`. `0.0.0`
representa el punto de partida de esta convención; no es la versión que
actualmente informa el compilador. En el código de este checkout, `--version`
informa `0.2.0+26fa6`: versión `0.2.0` y snapshot `26fa6`.

## Cuándo cambiar cada número

- **MAJOR:** cambios incompatibles después de alcanzar `1.0.0`. La primera
  versión estable será `1.0.0`, cuando el lenguaje y sus interfaces públicas
  se consideren suficientemente estables.
- **MINOR:** nuevas capacidades del lenguaje o cambios incompatibles mientras
  la versión mayor siga en `0`. Antes de `1.0.0`, el lenguaje todavía puede
  cambiar; por ejemplo, una nueva etapa de desarrollo puede pasar de `0.1.0`
  a `0.2.0`. La incorporación de literales de mapa con llaves es un cambio de
  esta clase.
- **PATCH:** correcciones de errores que no cambian intencionadamente la
  sintaxis ni el comportamiento público compatible. Por ejemplo, la siguiente
  corrección de `0.2.0` sería `0.2.1`.

Los números no se reinician al cambiar de componente: una corrección de
`0.1.0` no vuelve a `0.0.1`. Las versiones describen el estado del compilador y
del lenguaje; no cuentan cuántos archivos o líneas cambiaron.

## Snapshots de desarrollo

Para identificar una compilación de desarrollo se puede añadir una etiqueta
de snapshot con el formato `YYfaN`: `YY` son los dos últimos dígitos del año,
`fa` identifica una iteración de corrección de fallos y `N` es su número
secuencial dentro de ese año. Por ejemplo, `26fa1` identifica la primera
iteración de corrección de fallos de 2026.

La etiqueta es independiente de la versión SemVer. Si se muestra junto a una
versión, se escribe como metadato, por ejemplo `0.1.0+26fa1`; no reemplaza ni
modifica el orden de versiones. Se incrementa `N` al preparar la siguiente
iteración de correcciones de ese año. Una snapshot no equivale por sí sola a
una versión publicada.

## Consultar y actualizar la versión

Consulta la versión del ejecutable con:

```sh
./build/rivo --version
```

Al publicar una versión nueva, actualiza `RIVO_VERSION` en `src/main.c` y
comprueba que `--version` y la ayuda muestran el valor esperado. Al preparar
otra compilación de desarrollo, actualiza `RIVO_SNAPSHOT` con el siguiente
identificador de la convención; el valor no se genera automáticamente.
