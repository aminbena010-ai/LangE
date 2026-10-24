# Matriz de soporte de Rivo

Estado de las plataformas para v0.8 (2026-10). El criterio de cierre de la
versión exige `make test` en verde en al menos dos sistemas distintos; la
columna **Estado** refleja lo verificado, no lo deseado.

## Plataformas

| Sistema | Arquitectura | Toolchain LLVM | Estado | Notas |
| --- | --- | --- | --- | --- |
| Linux | x86_64 | LLVM 17–22 (`llvm-dev`) | **Soportado** | Plataforma de referencia; suite completa + runtime ASan/UBSan |
| Linux | ARM64 | LLVM 17–22 | **Soportado** (CI) | Runner nativo `ubuntu-24.04-arm` |
| macOS | ARM64 / x86_64 | LLVM de Homebrew | **Soportado** (CI) | `install.sh` usa `brew --prefix llvm` |
| Windows | x64 | LLVM/Clang (winget o choco) | **Experimental** | Compila y pasa la suite, salvo los tests de intercalado de fibras |
| Windows | ARM64 | LLVM/Clang | **Experimental** | Sin verificación en CI |

La matriz se ejecuta en `.github/workflows/ci.yml`:

- `Linux x86_64`: `make test` + `make test-runtime`.
- `Linux ARM64`: `make test`.
- `macOS`: `make test`.
- `Windows x64`: `make test` (job no bloqueante, marcado como experimental).

## Limitaciones conocidas

- **Windows: fibras síncronas.** No hay `ucontext` en Windows, así que
  `async`/`await` usa el respaldo ansioso: la tarea se ejecuta al llamarla y
  no hay suspensión real en `await`. Por eso `tests/run_tests.sh` omite en
  Windows los casos que dependen del intercalado (`async_intercalado`,
  `async_try_fibras`, `async_sin_await`). Ver
  [async](../language/async.md).
- **Una sola hebra.** El planificador es cooperativo y FIFO; una operación
  bloqueante detiene todas las tareas. Hilos y paralelismo quedan fuera de
  v0.8.
- **Enlazado externo.** El compilador invoca `clang` para enlazar, por lo que
  el driver debe estar en `PATH`. Si falta, el error es
  `no se encontró el linker 'clang'`.

## Versiones de LLVM

El proyecto busca LLVM con `LLVMConfig.cmake` y, si no está en la ruta de
CMake, con `llvm-config` (`llvm-config`, `llvm-config-22` … `-17`). Se
recomienda la misma versión de LLVM con la que se compiló el binario para
enlazar programas (biblioteca compartida de LLVM disponible en runtime).

## Cómo verificar una plataforma

```sh
make build
make test
make test-runtime   # solo en plataformas con ASan/UBSan (Linux/macOS)
./build/rivo --version
```
