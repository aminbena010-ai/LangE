# Instalación

## Compilar desde el repositorio

Se requieren CMake 3.16+, un compilador C11, LLVM con archivos de desarrollo y
`clang` para enlazar los ejecutables generados.

```sh
cmake -S . -B build
cmake --build build
./build/rivo --version
```

## Instalar en el usuario

Los instaladores compilan el checkout local y colocan el compilador, el runtime
y la biblioteca estándar en el directorio del usuario.

### Linux y macOS

Desde la raíz del proyecto:

```sh
./install.sh
```

En Linux, el script instala las herramientas de build que falten usando `apt`,
`dnf` o `pacman`, crea un paquete `.tar.gz` firmado en `dist/` y lo instala.
GPG puede solicitar una frase de contraseña para crear una clave local. Para
verificar paquetes distribuidos, comprueba el fingerprint por un canal
independiente y confiable. Linux admite `x86_64` y `aarch64`.

En macOS, el script usa Homebrew para CMake y LLVM; instala directamente el
checkout y admite `x86_64` y Apple Silicon (`arm64`). Homebrew y las
herramientas de línea de comandos de Xcode deben estar disponibles.

`PREFIX=/otra/ruta ./install.sh` cambia el destino. Ninguna variante descarga
el código fuente remoto.

### Windows nativo

Abre PowerShell en la raíz del proyecto:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

El instalador usa `winget` y configura CMake, LLVM y Visual Studio Build Tools
con el Windows SDK cuando hacen falta. La instalación se realiza en
`%LOCALAPPDATA%\Programs\Rivo`; abre una nueva terminal al terminar para
actualizar `PATH`. Se requiere acceso a Internet para instalar estas
dependencias.

También puede instalarse directamente con Make en Linux y macOS:

```sh
make install
rivo --help
```

Por defecto instala en `~/.local`: `bin/rivo`, `share/rivo/lib` y
`lib/librivo_runtime.a`. Si `~/.local/bin` no está en `PATH`, añade ese
directorio al `PATH` de tu shell. Un prefijo alternativo se indica con
`make install PREFIX=/ruta`; `make uninstall` retira esa instalación.

## Comprobar la instalación

Guarda [hola mundo](hello-world.md) como `hola.ro`, compílalo con
`rivo hola.ro -o hola` y ejecuta `./hola`.

Si el directorio lleva un manifiesto [`rivo.toml`](../compiler/manifest.md)
con `entrada`, basta con `rivo` (y `rivo --run` para compilar y ejecutar);
los artefactos quedan en `target/<perfil>/`.

## Verificación de paquetes

Los paquetes de distribución se firman con la clave de release del proyecto.
La clave pública está en [`keys/rivo-release-key.asc`](../../keys/rivo-release-key.asc)
y el procedimiento de verificación (fingerprint, GPG y `SHA256SUMS`) se
describe en [SECURITY.md](../../SECURITY.md).
