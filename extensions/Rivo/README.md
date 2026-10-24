# Rivo — soporte completo de E++ para VSCode

Extensión unificada para el lenguaje **Rivo/E++** (ficheros `.ro`):
sustituye a las antiguas `rcolor` + `rerror` + `resencial` con todo
integrado en un solo servidor de lenguaje.

## Qué incluye

- **Resaltado de sintaxis** con scopes estándar (colores de tu tema)
  + **resaltado semántico** de variables, parámetros, funciones y módulos.
  Cubre `struct`, tipos y campos, `elif`, `async`/`await`/`task`, `//`,
  `++`/`--` y f-strings.
- **Autocompletado inteligente**:
  - variables y parámetros declarados (con su tipo),
  - funciones del fichero con firmas,
  - 44 funciones incorporadas (`print`, `to_float`, `range`…) con documentación,
  - 28 módulos stdlib: tras `math.` aparecen sus funciones (`square_root`, …),
  - los structs declarados y sus campos tras `instancia.`,
  - las palabras clave y tipos integrados, con documentación y ejemplo.
- **Soporte de structs**: campos en el outline, autocompletado y navegación
  a la declaración del campo; hover de campos y tipos, y ayuda de firma del
  constructor posicional.
- **Hover con documentación de cada palabra reservada** (`fn`, `async`, `await`,
  `try`…), de cada tipo (`int`, `task`, `list`…) y de cada operador
  (`//`, `++`, `->`, `+=`…), con ejemplo de uso.
- **Ir a definición (Ctrl+clic)**: decls locales, y desde `math.square_root`
  salta directo a `lib/math.ro` del compilador.
- **Hover** con firma y doc de funciones, variables, builtins y módulos.
- **Errores en vivo** en el panel *Problems* (usa el compilador `rivo`).
  Las compilaciones de diagnóstico van a un directorio temporal:
  **nunca deja `.o` en tu proyecto**.
- **Outline/breadcrumbs** con funciones, variables e imports.
- **SignatureHelp** al escribir llamadas (parámetro activo resaltado).
- **Referencias (Shift+F12)** en el fichero y en todo el workspace.
- **Snippets** (`main`, `fn`, `afn` (async), `ifel`, `for`, `try`, `list`, `map`, …).
- **Ejecutar (▷ o Ctrl+F5)** y **Compilar**: notificación de progreso, los
  binarios van a `<workspace>/.rivo/` (configurable) y el `.gitignore` se
  actualiza solo. La ejecución usa una terminal interactiva reutilizable
  (`input()` funciona). Si el programa (o un módulo local que importa) usa
  `import mgl`, se añaden solos los flags de enlace `-l X11 -l Xft
  -l fontconfig`.
- **Barra de estado** con la versión del compilador (clic → reiniciar el
  servidor de lenguaje; si falta el compilador, clic → abrir ajustes).

## Configuración

| Opción | Por defecto | Descripción |
| --- | --- | --- |
| `rivo.compilerPath` | `rivo` | Ruta al binario del compilador |
| `rivo.buildDir` | `.rivo` | Carpeta de salida de binarios/objetos |
| `rivo.manageGitignore` | `true` | Añadir la carpeta de build al `.gitignore` |
| `rivo.diagnostics` | `true` | Errores en vivo en el panel Problems |
| `rivo.semanticTokens` | `true` | Resaltado semántico con tu tema |

## Comandos

- `Rivo: Ejecutar fichero` (▷ en la barra de título, `Ctrl+F5`)
- `Rivo: Compilar fichero`
- `Rivo: Reiniciar servidor de lenguaje` (también desde la barra de estado)
- `Rivo: Abrir ajustes de Rivo`

## Solución de problemas

### `Rivo: errores de compilación (detalle en el canal "Rivo")` y `.rivo/` vacío

El directorio de build se crea, pero no aparece el binario: la compilación
falló antes de generar la salida. Abre el canal **Salida → Rivo** para ver
el motivo real (ahí va stdout/stderr del compilador).

Causa habitual: se está usando un **compilador antiguo con CLI de
subcomandos** (`rivo build archivo.ro ...`, "Rivo - Compilador v0.1"). La
extensión invoca `rivo <archivo.ro> -o <salida>`; con esa CLI antigua el
fichero se interpreta como subcomando y aparece
`error: comando desconocido: <ruta>.ro`.

Para corregirlo, fija la ruta absoluta del compilador nuevo:

```jsonc
// settings.json (usuario o del workspace)
"rivo.compilerPath": "/home/usuario/.local/bin/rivo"
```

Comprobaciones:

- `rivo --version` debe ser ≥ 0.2.0 (CLI `rivo archivo.ro -o salida`,
  sin subcomandos `build`/`run`/`ir`).
- Si no defines `rivo.compilerPath`, se usa `rivo` del `PATH` de VS Code.
  Un VS Code instalado como snap/flatpak no hereda el `PATH` del shell y
  puede resolver otro binario (p. ej. uno antiguo en `/usr/local/bin`).
- Tras cambiar la opción, reinicia el servidor de lenguaje
  (`Rivo: Reiniciar servidor de lenguaje`) para que también la usen los
  diagnósticos en vivo.

Nota: la clave antigua `rerror.compilerPath` (extensión `rerror`
predecesora) **no** la lee esta extensión; renómbrala a
`rivo.compilerPath`.

### El fichero tiene errores reales de sintaxis

Si el canal muestra `SyntaxError`/`TypeError` con archivo y línea, el
problema es del `.ro`, no de la configuración. En ese caso no se genera
binario (y por eso `.rivo/` queda sin contenido nuevo). Corrige los
errores listados en el canal o en el panel *Problems* y vuelve a
ejecutar.

## Desarrollo

```sh
# datos de autocompletado/hover (regenerar tras tocar builtins.h, claude.md o lib/)
node scripts/gen-stdlib-data.js

# tests end-to-end del language server (sin VSCode)
node scripts/test-server.js

# empaquetar
npx --yes @vscode/vsce package --no-dependencies
code --install-extension rivo-1.4.0.vsix
```

## Estructura

```
extension.js          entrada (activa cliente, comandos y barra de estado)
client.js             cliente LSP mínimo (providers de VSCode)
runner.js             compilar/ejecutar en .rivo/ + gitignore
language-configuration.json  comentarios # y #! !#, indentación tras ":"
syntaxes/rivo.tmLanguage.json
snippets/rivo.json
server/server.js      servidor LSP completo
server/symbols.js     analizador heurístico (decls, usos, tokens)
server/parser.js      salida del compilador → Diagnostic
server/keywords.js    doc de palabras reservadas, tipos y operadores
server/stdlib-data.js generado (builtins + módulos)
```
