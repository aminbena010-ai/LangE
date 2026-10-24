# Compilar Rivo desde el código fuente

Requisitos: CMake 3.16+, compilador C11, LLVM con headers y `clang`. En
Windows nativo también se requiere el Windows SDK y un toolchain de Visual
Studio; `install.ps1` puede instalarlos con `winget`.

```sh
cmake -S . -B build
cmake --build build
./build/rivo --help
```

La configuración falla si no encuentra LLVM. Cuando LLVM no está en la ruta de
CMake, el proyecto intenta localizar `llvm-config`; configura `LLVM_DIR` si
necesitas indicar el paquete manualmente.

En macOS se detecta el linker nativo y se obtiene la ruta del ejecutable con
`_NSGetExecutablePath`. En Windows se usan las APIs de consola, reloj y ruta
del ejecutable de Win32; LLVM/Clang enlaza los programas resultantes usando el
Windows SDK.

## Plataformas

El estado por sistema y arquitectura, con las limitaciones conocidas (fibras
en Windows, versiones de LLVM), está en la
[matriz de soporte](support.md).
