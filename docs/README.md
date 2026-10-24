# Documentación de Rivo

Rivo (binario `rivo`; extensión fuente `.ro`) es un lenguaje de tipos estáticos,
con palabras clave en inglés, bloques por indentación y compilador LLVM. Esta
guía describe la implementación de este repositorio; ante cualquier diferencia,
manda el código y las pruebas de la versión que estés usando.

## Índice

- **Introducción:** [Qué es Rivo](introduction/what-is-rivo.md) · [Filosofía](introduction/philosophy.md) · [Instalación](introduction/installation.md) · [Hola, mundo](introduction/hello-world.md)
- **Lenguaje:** [Sintaxis](language/syntax.md) · [Indentación](language/indentation.md) · [Comentarios](language/comments.md) · [Identificadores](language/identifiers.md) · [Palabras clave](language/keywords.md)
- **Tipos:** [Resumen](types/overview.md) · [`int`](types/int.md) · [`float`](types/float.md) · [`string`](types/string.md) · [`bool`](types/bool.md) · [`char`](types/char.md) · [`list<T>`](types/list.md) · [`map<K,V>`](types/map.md) · [`struct`](types/struct.md) · [`null`](types/null.md)
- **Variables:** [`var`](variables/var.md) · [`const`](variables/const.md) · [Asignación](variables/assignment.md) · [Visibilidad](variables/visibility.md)
- **Operadores:** [Aritmética](operators/arithmetic.md) · [Comparación](operators/comparison.md) · [Lógica](operators/logical.md) · [Asignación compuesta](operators/compound-assignment.md) · [Precedencia](operators/precedence.md)
- **Control:** [`if`/`else`](control-flow/if-else.md) · [`while`](control-flow/while.md) · [`for`](control-flow/for.md) · [`break`/`continue`](control-flow/break-continue.md) · [`pass`](control-flow/pass.md)
- **Funciones:** [Declaración](functions/declaration.md) · [Parámetros](functions/parameters.md) · [Retorno](functions/return.md) · [Visibilidad](functions/visibility.md) · [`main`](functions/main.md)
- **Colecciones:** [Listas](collections/lists.md) · [Mapas](collections/maps.md) · [Iteración](collections/iteration.md) · [Concatenación](collections/concatenation.md)
- **Strings:** [Literales](strings/literals.md) · [Escapes](strings/escapes.md) · [Concatenación](strings/concatenation.md) · [F-strings](strings/f-strings.md)
- **Errores:** [`try`/`catch`](errors/try-catch.md) · [Errores de runtime](errors/runtime-errors.md) · [Diagnósticos](errors/diagnostics.md)
- **Módulos:** [`import`](modules/import.md) · [`from ... import`](modules/from-import.md) · [Alias](modules/alias.md) · [Búsqueda](modules/module-search.md) · [Namespaces](modules/namespaces.md)
- **Funciones incorporadas:** [Consola](builtins/console.md) · [Colecciones](builtins/collections.md) · [Texto](builtins/text.md) · [Conversión](builtins/conversion.md) · [Matemáticas](builtins/math.md) · [Tiempo](builtins/time.md) · [Aleatoriedad](builtins/random.md) · [Archivos](builtins/files.md) · [Terminal](builtins/terminal.md)
- **Biblioteca estándar:** [math](stdlib/math.md) · [text](stdlib/text.md) · [time](stdlib/time.md) · [random](stdlib/random.md) · [files](stdlib/files.md) · [json](stdlib/json.md) · [date](stdlib/date.md) · [system](stdlib/system.md) · [statistics](stdlib/statistics.md) · [validation](stdlib/validation.md) · [csv](stdlib/csv.md) · [paths](stdlib/paths.md) · [color](stdlib/color.md) · [codecs](stdlib/codecs.md) · [re](stdlib/re.md) · [strings](stdlib/strings.md) · [textwrap](stdlib/textwrap.md) · [difflib](stdlib/difflib.md) · [mgl](stdlib/mgl.md) · [Módulos con nombres de Python](stdlib/python-module-names.md)
- **Módulos Python añadidos:** [string](stdlib/python-module-names.md#string) · [os](stdlib/python-module-names.md#os) · [pathlib](stdlib/python-module-names.md#pathlib) · [datetime](stdlib/python-module-names.md#datetime-y-calendar) · [platform](stdlib/python-module-names.md#platform) · [base64](stdlib/python-module-names.md#base64) · [calendar](stdlib/python-module-names.md#datetime-y-calendar) · [fnmatch](stdlib/python-module-names.md#fnmatch) · [html](stdlib/python-module-names.md#html) · [shutil](stdlib/python-module-names.md#shutil)
- **FFI:** [`external`](ffi/external.md) · [Tipos permitidos](ffi/allowed-types.md) · [Build y enlace](ffi/build-and-link.md) · [Ejemplos](ffi/examples.md)
- **Paquetes:** [Gestor de paquetes](modules/packages.md)
- **Compilador:** [CLI](compiler/cli.md) · [Opciones](compiler/options.md) · [Manifiesto](compiler/manifest.md) · [Build](compiler/build.md) · [Pipeline](compiler/pipeline.md) · [Arquitectura](compiler/architecture.md)
- **Runtime:** [Memoria](runtime/memory.md) · [Arena](runtime/arena.md) · [Colecciones](runtime/collections.md) · [Errores](runtime/errors.md)
- **Desarrollo:** [Build](development/building.md) · [Estructura](development/project-structure.md) · [Pruebas](development/testing.md) · [Depuración](development/debugging.md) · [Versionado](development/versioning.md) · [Contribuir](development/contributing.md)
- **Referencia:** [Palabras clave](reference/keywords.md) · [Tokens](reference/tokens.md) · [AST](reference/ast.md) · [Sistema de tipos](reference/type-system.md) · [Clases de error](reference/error-classes.md) · [Hoja rápida](reference/cheat-sheet.md)
- **Ejemplos:** [Hola mundo](examples/hello-world.md) · [Calculadora](examples/calculator.md) · [FizzBuzz](examples/fizzbuzz.md) · [Fibonacci](examples/fibonacci.md) · [Ordenación](examples/sorting.md) · [Contar palabras](examples/word-count.md) · [Menú interactivo](examples/interactive-menu.md) · [Adivinar palabra](examples/hangman.md)
- **Guías:** [Desde Python](guides/from-python.md) · [Desde C](guides/from-c.md) · [Desde JavaScript](guides/from-javascript.md) · [Ruta de aprendizaje](guides/learning-path.md)
- **Apéndice:** [Glosario](appendix/glossary.md) · [FAQ](appendix/faq.md) · [Registro](appendix/changelog.md) · [Estado](appendix/roadmap.md)
- **Internals:** [Lexer](internals/lexer.md) · [Parser](internals/parser.md) · [AST](internals/ast.md) · [Semántica](internals/semantics.md) · [LLVM](internals/llvm.md) · [Linker](internals/linker.md) · [Errores](internals/error-system.md)

## Compilar y ejecutar

```sh
cmake -S . -B build
cmake --build build
./build/rivo programa.ro -o programa
./programa
```

Las páginas con ejemplos presentan programas completos cuando es pertinente;
guárdalos como `.ro` y compílalos con el comando anterior. Cómo ejecutar y
ampliar la suite de [pruebas](development/testing.md); no todas las API de la
biblioteca estándar tienen cobertura equivalente.

## Estructura

Las páginas están agrupadas por concepto; [el árbol de navegación](README.md)
es el índice. La documentación histórica del proyecto está en
[`document/DOCUMENTACION.md`](../document/DOCUMENTACION.md), pero algunas de sus
afirmaciones pueden estar desactualizadas. Los nombres de tipos son `int`,
`float`, `string`, `bool`, `char`, `list<T>`, `map<K, V>`, `struct`,
`task<T>` y `null`.
