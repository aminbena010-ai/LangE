# Depuración

Usa `-g` para incluir información de depuración y `-v` para mostrar
información adicional del compilador. LLVM IR se puede inspeccionar con
`-emit-ir`; también se puede conservar el objeto intermedio.

```sh
./build/rivo programa.ro -g --keep-object -o programa
./build/rivo programa.ro -emit-ir
```

En proyectos con manifiesto [`rivo.toml`](../compiler/manifest.md) el objeto
se conserva siempre en `target/<perfil>/` y `-v` muestra además el tiempo de
cada fase (véase [build](../compiler/build.md)).

Para errores de runtime, reduce el caso a un programa corto y ejecútalo fuera
del compilador. Las pruebas negativas de `tests/errors/` muestran cómo se
verifican diagnósticos.
