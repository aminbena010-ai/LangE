# E++ (Rivo)

**E++** es un lenguaje de programación compilado, de tipado estático y sintaxis
en español (con palabras clave en inglés). Su compilador se llama **`rivo`**, usa
LLVM para generar código nativo y gestiona la memoria automáticamente mediante
un runtime con arena: nunca se llama a `free`.

> Estado: **pre-1.0** (v0.8.0+08a2). El lenguaje aún puede cambiar sin
> garantías de compatibilidad.

## Características

- Tipado estático, compilación nativa vía LLVM, extensión `.ro`.
- Gestión de memoria automática con arena (sin `free` manual).
- Diagnósticos en español con sugerencias, y avisos (p. ej. variable sin usar)
  convertibles en errores.
- `async`/`await` con fibras `ucontext` y planificador FIFO cooperativo.
- FFI con C: enteros de ancho fijo, `opaque`, strings con propiedad,
  callbacks, structs POD y verificación de ABI.
- Gestor de paquetes con `rivo.toml`, `rivo.lock` e índice Git.
- Herramientas: `rivo fmt`, LSP y extensión de VS Code.
- Caché de compilación por huella SHA-256 y niveles `-O0`–`-O3`.

## Inicio rápido

Requisitos: CMake ≥ 3.16, compilador C11, LLVM con headers de desarrollo y
`clang` para enlazar programas generados.

```sh
cmake -S . -B build
cmake --build build
./build/rivo --version
```

Un proyecto puede llevar un manifiesto `rivo.toml` (nombre, versión, entrada,
dependencias, binario, perfil y opciones por defecto) que el compilador
descubre automáticamente. Con manifiesto, los artefactos van a
`target/debug/` o `target/release/` y `rivo --run` compila y ejecuta en una
pasada. Véanse [docs/compiler/manifest.md](docs/compiler/manifest.md) y
[docs/compiler/build.md](docs/compiler/build.md).

Hola mundo (`hola.ro`):

```rivo
fn main():
    print("Hola mundo")
```

```sh
rivo hola.ro --run
```

## Instalar

Desde la raíz del proyecto, el instalador prepara las dependencias, compila
este checkout e instala Rivo en tu usuario:

```sh
./install.sh
```

En Windows, desde PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

Los instaladores compilan el código local; no descargan el proyecto. En Linux
pueden instalar CMake, LLVM, Clang y GPG con el gestor de paquetes del sistema;
en macOS usan Homebrew (debe estar instalado) y las herramientas de Xcode; en
Windows usan `winget` para CMake, LLVM y Visual Studio Build Tools. El prefijo
predeterminado es `~/.local` en Linux/macOS y `%LOCALAPPDATA%\Programs\Rivo` en
Windows. En Linux, `install.sh` también genera un paquete local firmado en
`dist/`; la firma protege ese paquete, no sustituye la verificación
independiente del fingerprint. `PREFIX=/otra/ruta ./install.sh` cambia el
destino.

También puede instalarse con Make en Linux y macOS:

```sh
make install                    # compila e instala en ~/.local
make install PREFIX=/otro/dir   # otro destino
make uninstall                  # desinstala
```

En Linux/macOS se instala `~/.local/bin/rivo` (compilador),
`~/.local/share/rivo/lib` (biblioteca estándar `.ro`) y
`~/.local/lib/librivo_runtime.a` (runtime). En Windows el runtime estático se
instala como `librivo_runtime.lib`. En el árbol de desarrollo el binario sigue
llamándose `build/rivo`.

Windows: tras instalar, abre una terminal nueva para actualizar `PATH`. El
primer build nativo requiere Windows 10/11, `winget` y permiso para instalar
Visual Studio Build Tools y el Windows SDK.

## Documentación

- [Guía completa en español](document/DOCUMENTACION.md).
- [Documentación por temas](docs/README.md).
- [Matriz de soporte de plataformas](docs/development/support.md).
- [Changelog](CHANGELOG.md).

## Web

El sitio del proyecto vive en [`web/`](web/): es 100 % estático (HTML + CSS +
JavaScript vanilla, sin build ni servidor propio) e incluye la presentación del
lenguaje, la documentación oficial y las guías *Python → Rivo* y
*Godot → Rivo*.

```sh
python3 -m http.server 8000 --directory web
```

Abre <http://127.0.0.1:8000> (también puedes abrir `web/index.html`
directamente).

## Pruebas y rendimiento

```sh
make test           # suite end-to-end
make test-runtime   # runtime con ASan/UBSan
make bench          # benchmarks base
```

## Estructura del proyecto

```text
src/          compilador (lexer, parser, semántica, LLVM, linker, CLI)
src/runtime/  runtime (arena, async, listas, mapas, errores)
include/      cabeceras públicas del compilador
lib/          biblioteca estándar (.ro y helpers en C)
tests/        pruebas e2e, de paquetes y del runtime
docs/         documentación por temas
extensions/   extensión de VS Code y LSP
web/          sitio web estático
keys/         clave pública de firma de releases
```

## Contribuir

Lee [CONTRIBUTING.md](CONTRIBUTING.md). Para vulnerabilidades, consulta
[SECURITY.md](SECURITY.md).

## Licencia

[MIT](LICENSE).
