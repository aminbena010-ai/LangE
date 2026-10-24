#!/bin/sh
#
# Tests end-to-end de E++: runtime + pipeline del compilador.
#
# Prerrequisito:
#     cmake -S . -B build
#     cmake --build build
#
# Uso:
#     sh tests/run_tests.sh

set -u

root=$(cd "$(dirname "$0")/.." && pwd)
build="${RIVO_BUILD:-$root/build}"
rivo="$build/rivo"

# Windows: el ejecutable lleva extensión .exe.
if [ ! -x "$rivo" ] && [ -x "$rivo.exe" ]; then
    rivo="$rivo.exe"
fi
tmp=$(mktemp -d)

trap 'rm -rf "$tmp"' EXIT

fails=0
passes=0

if [ ! -x "$rivo" ]; then
    echo "No se encuentra $rivo. Ejecute antes:" >&2
    echo "  cmake -S . -B build && cmake --build build" >&2
    exit 1
fi


# -------------------------------------------------------
# Compila, ejecuta y compara la salida estándar.
# -------------------------------------------------------

run_ok() {
    name=$1
    file=$2
    stdin_data=$3
    expected=$4

    # Argumentos extra para rivo (p. ej. "-I dir"); opcional.
    extra_args=${5:-}

    if ! "$rivo" "$file" -o "$tmp/$name.bin" $extra_args \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (la compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    printf '%s' "$stdin_data" \
        | "$tmp/$name.bin" > "$tmp/$name.out" 2>&1

    printf '%s' "$expected" > "$tmp/$name.expected"

    if ! diff -u "$tmp/$name.expected" "$tmp/$name.out" \
            > "$tmp/$name.diff" 2>&1; then
        echo "FAIL  $name (salida inesperada)"
        sed 's/^/      /' "$tmp/$name.diff"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Programa inválido: debe rechazarse con el mensaje dado
# y no debe producir ejecutable.
# -------------------------------------------------------

run_err() {
    name=$1
    file=$2
    expected_text=$3

    if "$rivo" "$file" -o "$tmp/$name.bin" \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (deberia haber sido rechazado)"
        fails=$((fails + 1))
        return
    fi

    if [ -f "$tmp/$name.bin" ]; then
        echo "FAIL  $name (genero ejecutable invalido)"
        fails=$((fails + 1))
        return
    fi

    if ! grep -q "$expected_text" "$tmp/$name.log"; then
        echo "FAIL  $name (mensaje inesperado)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Igual que run_err pero con flags extra del compilador
# (ej. --ffi-check).
# -------------------------------------------------------

run_err_flags() {
    name=$1
    file=$2
    expected_text=$3
    shift 3

    if "$rivo" "$@" "$file" -o "$tmp/$name.bin" \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (deberia haber sido rechazado)"
        fails=$((fails + 1))
        return
    fi

    if ! grep -q "$expected_text" "$tmp/$name.log"; then
        echo "FAIL  $name (mensaje inesperado)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Programa que compila pero falla en ejecución:
# debe terminar con un código distinto de 0 y mostrar el
# mensaje del runtime (decisión de diseño 2026-10-06:
# índice fuera de rango y clave inexistente abortan).
# -------------------------------------------------------

run_fail() {
    name=$1
    file=$2
    expected_text=$3

    if ! "$rivo" "$file" -o "$tmp/$name.bin" \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (la compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    "$tmp/$name.bin" > "$tmp/$name.out" 2>&1
    status=$?

    if [ "$status" -eq 0 ]; then
        echo "FAIL  $name (el programa no fallo)"
        sed 's/^/      /' "$tmp/$name.out"
        fails=$((fails + 1))
        return
    fi

    if ! grep -q "$expected_text" "$tmp/$name.out"; then
        echo "FAIL  $name (mensaje inesperado)"
        sed 's/^/      /' "$tmp/$name.out"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Programa bloqueado por un problema conocido.
#
# Si deja de estar bloqueado se avisa: hay que actualizar
# TODO.md y error.log.
# -------------------------------------------------------

run_blocked() {
    name=$1
    file=$2
    reason=$3

    if "$rivo" "$file" -o "$tmp/$name.bin" \
            > "$tmp/$name.log" 2>&1; then
        echo "REVISED  $name (dejo de estar bloqueado: $reason)"
        return
    fi

    echo "BLOCKED  $name ($reason)"
}


# -------------------------------------------------------
# Compila SIN archivo de entrada: el manifiesto rivo.toml
# del directorio $2 declara `entrada`. Se ejecuta desde
# ese directorio, como haría un usuario del proyecto.
# -------------------------------------------------------

run_no_input_ok() {
    name=$1
    dir=$2
    expected=$3

    if ! (cd "$dir" && "$rivo" -o "$tmp/$name.bin") \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (la compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    "$tmp/$name.bin" > "$tmp/$name.out" 2>&1

    printf '%s' "$expected" > "$tmp/$name.expected"

    if ! diff -u "$tmp/$name.expected" "$tmp/$name.out" \
            > "$tmp/$name.diff" 2>&1; then
        echo "FAIL  $name (salida inesperada)"
        sed 's/^/      /' "$tmp/$name.diff"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Compila SIN -o (usa target/ del manifiesto), comprueba
# el binario en la ruta dada (relativa al directorio del
# proyecto) y lo ejecuta.
# -------------------------------------------------------

run_no_input_bin_ok() {
    name=$1
    dir=$2
    binary_rel=$3
    expected=$4

    if ! (cd "$dir" && "$rivo") \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (la compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    if [ ! -x "$dir/$binary_rel" ]; then
        echo "FAIL  $name (no existe $binary_rel)"
        fails=$((fails + 1))
        return
    fi

    "$dir/$binary_rel" > "$tmp/$name.out" 2>&1

    printf '%s' "$expected" > "$tmp/$name.expected"

    if ! diff -u "$tmp/$name.expected" "$tmp/$name.out" \
            > "$tmp/$name.diff" 2>&1; then
        echo "FAIL  $name (salida inesperada)"
        sed 's/^/      /' "$tmp/$name.diff"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


# -------------------------------------------------------
# Compila dos veces SIN -o: la segunda debe reutilizar
# la caché de target/ ("sin cambios").
# -------------------------------------------------------

run_no_input_cached() {
    name=$1
    dir=$2

    if ! (cd "$dir" && "$rivo") \
            > "$tmp/$name.log" 2>&1; then
        echo "FAIL  $name (la primera compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.log"
        fails=$((fails + 1))
        return
    fi

    if ! (cd "$dir" && "$rivo") \
            > "$tmp/$name.2.log" 2>&1; then
        echo "FAIL  $name (la segunda compilacion fallo)"
        sed 's/^/      /' "$tmp/$name.2.log"
        fails=$((fails + 1))
        return
    fi

    if ! grep -q "sin cambios" "$tmp/$name.2.log"; then
        echo "FAIL  $name (no uso la caché)"
        sed 's/^/      /' "$tmp/$name.2.log"
        fails=$((fails + 1))
        return
    fi

    echo "PASS  $name"
    passes=$((passes + 1))
}


echo "=== Tests de runtime (C directo) ==="

if sh "$root/tests/runtime/run_tests.sh"; then
    passes=$((passes + 1))
else
    fails=$((fails + 1))
fi


echo ""
echo "=== Tests del gestor de paquetes ==="

if sh "$root/tests/packages/run_tests.sh"; then
    passes=$((passes + 1))
else
    fails=$((fails + 1))
fi


echo ""
echo "=== Tests de integracion (.ro) ==="

run_ok hello \
    "$root/tests/integration/hello.ro" \
    "" \
    "Hola mundo
"

run_ok basic_hello \
    "$root/tests/basic/hello.ro" \
    "" \
    "Hola mundo
"

run_ok async_basico \
    "$root/tests/integration/async_basico.ro" \
    "" \
    "42
-1
1
"

run_ok async_tipos \
    "$root/tests/integration/async_tipos.ro" \
    "" \
    "Rivo
2.5
true
3
7
"

run_ok async_anidado \
    "$root/tests/integration/async_anidado.ro" \
    "" \
    "21
106
hola async
"

case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) sin_fibras_reales=1 ;;
    *) sin_fibras_reales=0 ;;
esac

if [ "$sin_fibras_reales" -eq 1 ]; then
    echo "SKIP  async_intercalado (Windows: fibras síncronas)"
else
    run_ok async_intercalado \
        "$root/tests/integration/async_intercalado.ro" \
        "" \
        "a-1
b-1
a-2
b-2
"
fi

if [ "$sin_fibras_reales" -eq 1 ]; then
    echo "SKIP  async_try_fibras (Windows: fibras síncronas)"
else
    run_ok async_try_fibras \
        "$root/tests/integration/async_try_fibras.ro" \
        "" \
        "a-try
b-try
c-try
a-catch
b-fin
interno
externo
"
fi

run_ok async_task_anidada \
    "$root/tests/integration/async_task_anidada.ro" \
    "" \
    "5
"

if [ "$sin_fibras_reales" -eq 1 ]; then
    echo "SKIP  async_sin_await (Windows: fibras síncronas)"
else
    run_ok async_sin_await \
        "$root/tests/integration/async_sin_await.ro" \
        "" \
        "main
tarea
"
fi

run_ok print_tipos \
    "$root/tests/integration/print_tipos.ro" \
    "" \
    "42
19.99
true
false
A
texto
"

run_ok decimal_exacto \
    "$root/tests/integration/decimal_exacto.ro" \
    "" \
    "0.30000000000000004
false
0.19999999999999998
0.2
1e-18
2e-18
19.99
3.0
0.5
0.0
3.5
1.0
-2.5
0.3333333333333333
19.99
v=2.5
0.30000000000000004
"

run_ok operators_report \
    "$root/tests/integration/operators_report.ro" \
    "" \
    "4
5
6
6
4
3
-4
-4
3
valores: 4 true 2.5 R
"

run_ok function_values \
    "$root/tests/integration/function_values.ro" \
    "" \
    "5
true
callable
"

run_ok structs \
    "$root/tests/integration/structs.ro" \
    "" \
    "Point(x=4.5, y=3.5)
Person(name=Ada, age=37, home=Point(x=4.5, y=3.5))
3.5
false
true
5.5
"

run_ok input \
    "$root/tests/integration/input.ro" \
    "E++" \
    "Ingresa: E++
"

run_ok len_texto \
    "$root/tests/integration/len_texto.ro" \
    "" \
    "4
0
"

run_ok control \
    "$root/tests/integration/control.ro" \
    "" \
    "42
5
7
false
true
99
si
"

run_ok for_lista \
    "$root/tests/integration/for_lista.ro" \
    "" \
    "15
1
2
4
________
ana
luis
90
7
100
"

run_ok listas \
    "$root/tests/integration/listas.ro" \
    "" \
    "4
4
1
4
9
3
14
"

run_ok len_lista \
    "$root/tests/integration/len_lista.ro" \
    "" \
    "3
"

run_ok maps \
    "$root/tests/integration/maps.ro" \
    "" \
    "2
21
25
Ana
21
Luis
25
3
dos
0
"

run_ok for_map \
    "$root/tests/integration/for_map.ro" \
    "" \
    "Ana
"

run_ok string_orden \
    "$root/tests/integration/string_orden.ro" \
    "" \
    "true
true
true
true
true
true
true
true
"

run_ok map_len_method \
    "$root/tests/integration/map_len_method.ro" \
    "" \
    "3
0
2
"

run_ok elif_basico \
    "$root/tests/integration/elif_basico.ro" \
    "" \
    "adolescente
"

run_ok elif_sin_else \
    "$root/tests/integration/elif_sin_else.ro" \
    "" \
    "tres
dos
"

run_ok elif_while \
    "$root/tests/integration/elif_while.ro" \
    "" \
    "cero
otro
dos
otro
cuatro
"

run_ok concat_string \
    "$root/tests/integration/concat_string.ro" \
    "Ana" \
    "Hola mundo
xy
123
vacio
Hola Ana
"

run_ok input_eof \
    "$root/tests/integration/input_eof.ro" \
    "" \
    "eof=
"

run_ok import_from \
    "$root/tests/integration/import_from.ro" \
    "" \
    "5
"

run_ok import_alias \
    "$root/tests/integration/import_alias.ro" \
    "" \
    "8
"

run_ok import_sin_alias \
    "$root/tests/integration/import_sin_alias.ro" \
    "" \
    "30
"

run_ok import_anidado \
    "$root/tests/integration/import_anidado.ro" \
    "" \
    "10
"

run_ok import_cadena \
    "$root/tests/integration/import_cadena.ro" \
    "" \
    "9
"

run_ok privacidad_dentro \
    "$root/tests/integration/privacidad_dentro.ro" \
    "" \
    "interno
privado
"


echo ""
echo "=== Tests negativos ==="

run_err print_sin_argumentos \
    "$root/tests/errors/print_sin_argumentos.ro" \
    "print() takes at least 1 argument"

run_err print_tipo_no_soportado \
    "$root/tests/errors/print_tipo_no_soportado.ro" \
    "print() only supports int, float, string"

run_err structs_campo_inexistente \
    "$root/tests/errors/structs_campo_inexistente.ro" \
    "campo desconocido en el struct"

run_err structs_tipo_invalido \
    "$root/tests/errors/structs_tipo_invalido.ro" \
    "tipo incompatible para el campo del struct"

run_err structs_tipo_no_declarado \
    "$root/tests/errors/structs_tipo_no_declarado.ro" \
    "tipo de struct no declarado"

run_err structs_recursivo \
    "$root/tests/errors/structs_recursivo.ro" \
    "los structs no pueden contenerse de forma recursiva"

run_err structs_campo_no_imprimible \
    "$root/tests/errors/structs_campo_no_imprimible.ro" \
    "print() no puede mostrar structs"

run_err len_int \
    "$root/tests/errors/len_int.ro" \
    "la longitud solo esta definida"

run_err elif_sin_dos_puntos \
    "$root/tests/errors/elif_sin_dos_puntos.ro" \
    "se esperaba COLON pero se encontro NEWLINE"

run_err elif_tras_else \
    "$root/tests/errors/elif_tras_else.ro" \
    "se esperaba una expresion"

run_err elif_condicion_no_bool \
    "$root/tests/errors/elif_condicion_no_bool.ro" \
    "la condicion de if debe ser bool"

run_err concat_tipo \
    "$root/tests/errors/concat_tipo.ro" \
    "incorrect argument type"

run_err variable_global \
    "$root/tests/errors/variable_global.ro" \
    "las variables y constantes se declaran dentro de una funcion"

run_err input_con_argumento \
    "$root/tests/errors/input_con_argumento.ro" \
    "input() prompt debe ser string"

run_err const_reasignacion \
    "$root/tests/errors/const_reasignacion.ro" \
    "cannot assign to a constant"

run_err concat_tipo_invalido \
    "$root/tests/errors/concat_tipo_invalido.ro" \
    "invalid operation for these types"

run_err catch_error_uso \
    "$root/tests/errors/catch_error_uso.ro" \
    "name not defined"

run_err try_sin_catch \
    "$root/tests/errors/try_sin_catch.ro" \
    "expected catch after try"

run_ok try_catch \
    "$root/tests/integration/try_catch.ro" \
    "" \
    "bloque 1
indice 10 fuera de rango en la linea 7 (longitud 3)
la clave \"noexiste\" no existe en el map en la linea 13
sin error
interno
interno
indice 98 fuera de rango en la linea 33 (longitud 3)
exterior:
indice 97 fuera de rango en la linea 36 (longitud 3)
fin
"

run_err try_salto \
    "$root/tests/errors/try_salto.ro" \
    "are not allowed inside try"

run_fail lista_indice \
    "$root/tests/errors/lista_indice.ro" \
    "indice 5 fuera de rango"

run_fail lista_indice_asignacion \
    "$root/tests/errors/lista_indice_asignacion.ro" \
    "indice 9 fuera de rango"

run_fail floor_div_zero \
    "$root/tests/integration/floor_div_zero.ro" \
    "ZeroDivisionError"

run_fail division_entera_cero \
    "$root/tests/integration/division_entera_cero.ro" \
    "ZeroDivisionError"

run_fail modulo_cero \
    "$root/tests/integration/modulo_cero.ro" \
    "ZeroDivisionError"

run_fail desbordamiento_mul \
    "$root/tests/integration/desbordamiento_mul.ro" \
    "OverflowError"

run_fail desbordamiento_suma \
    "$root/tests/integration/desbordamiento_suma.ro" \
    "OverflowError"

run_ok desbordamiento_try \
    "$root/tests/integration/desbordamiento_try.ro" \
    "" \
    'capturado
4294967296
'

run_fail map_clave_inexistente \
    "$root/tests/errors/map_clave_inexistente.ro" \
    "no existe en el map"

run_err map_len_args \
    "$root/tests/errors/map_len_args.ro" \
    "mapa.len() no recibe argumentos"

run_err import_no_existe \
    "$root/tests/errors/import_no_existe.ro" \
    "module not found"

run_err import_privado \
    "$root/tests/errors/import_privado.ro" \
    "only public functions can be imported"

run_err import_calificado_privado \
    "$root/tests/errors/import_calificado_privado.ro" \
    "only public functions of a module can be called"

run_err import_funcion_inexistente \
    "$root/tests/errors/import_funcion_inexistente.ro" \
    "el modulo 'math' no tiene la funcion 'noexiste'"

run_err import_calificado_inexistente \
    "$root/tests/errors/import_calificado_inexistente.ro" \
    "el modulo 'math' no tiene la funcion 'nox'"

run_err import_main \
    "$root/tests/errors/import_main.ro" \
    "no se permite 'main' en un modulo importado"

run_err import_circular \
    "$root/tests/errors/import_circular.ro" \
    "import circular detectado"

run_err import_en_funcion \
    "$root/tests/errors/import_en_funcion.ro" \
    "imports are only allowed at global level"

run_err import_colision \
    "$root/tests/errors/import_colision.ro" \
    "la funcion ya esta declarada"

run_err import_alias_duplicado \
    "$root/tests/errors/import_alias_duplicado.ro" \
    "el alias ya esta declarado"

run_err import_nombre_duplicado \
    "$root/tests/errors/import_nombre_duplicado.ro" \
    "la funcion ya esta declarada"

run_err async_await_no_task \
    "$root/tests/errors/async_await_no_task.ro" \
    "await requiere una tarea task<T>"

run_err async_main \
    "$root/tests/errors/async_main.ro" \
    "main no puede ser async"

run_err async_external \
    "$root/tests/errors/async_external.ro" \
    "se esperaba FN"


echo ""
echo "=== Funciones incorporadas (fase 2.1) ==="

run_ok builtins_texto \
    "$root/tests/integration/builtins_texto.ro" \
    "" \
    "HOLA E++
hola e++
espacio
defg
TUNel TUNel
true
true
true
3
-1
3
tres
uno dos tres
2
a-b
"

run_ok concat_builtin \
    "$root/tests/integration/concat.ro" \
    "" \
    "hola E++
abc
"

run_ok builtins_aleatorio \
    "$root/tests/integration/builtins_aleatorio.ro" \
    "" \
    "true
true
true
true
true
true
true
"

# clear() y move() emiten secuencias ANSI: en la salida
# del test se comparan con los escapes reales.
run_ok builtins_consola \
    "$root/tests/integration/builtins_consola.ro" \
    "K" \
    "$(printf '\033')[2J$(printf '\033')[H$(printf '\033')[3;5Hlisto
K
"

run_ok builtins_ficheros \
    "$root/tests/integration/builtins_ficheros.ro" \
    "" \
    "true
true
contenido de prueba
false
"

run_ok literal_vacio \
    "$root/tests/integration/literal_vacio.ro" \
    "" \
    "0
0
1
0
0
0
"

run_err lista_vacio_sin_anotacion \
    "$root/tests/errors/lista_vacio_sin_anotacion.ro" \
    "cannot infer the type of an empty list"

run_err builtin_arity \
    "$root/tests/errors/builtin_arity.ro" \
    "random() takes exactly 2 arguments"

run_err builtin_tipo \
    "$root/tests/errors/builtin_tipo.ro" \
    "incorrect argument type"

run_fail read_file_inexistente \
    "$root/tests/errors/read_file_inexistente.ro" \
    "no se pudo leer el archivo"

run_fail random_inverso \
    "$root/tests/errors/random_inverso.ro" \
    "random: el minimo 10 es mayor que el maximo 1"


echo ""
echo "=== Funciones externas de C ==="

run_ok external_c \
    "$root/tests/integration/external_c.ro" \
    "" \
    "5
Hola E++
[E++]
false
true
6
"

run_ok external_modulo \
    "$root/tests/integration/external_modulo.ro" \
    "" \
    "42
Hola modulo
"

run_err external_tipo \
    "$root/tests/errors/external_tipo.ro" \
    "los parametros de una funcion externa deben ser int"

run_err external_retorno \
    "$root/tests/errors/external_retorno.ro" \
    "el retorno de una funcion externa debe ser int"

run_err external_fichero \
    "$root/tests/errors/external_fichero.ro" \
    "no se pudo compilar el fichero C"

run_ok external_int_widths \
    "$root/tests/integration/external_int_widths.ro" \
    "" \
    "20
2000
200000
200
60000
2000000
2000000000
21
255
-128
"

run_ok external_void_explicit \
    "$root/tests/integration/external_void_explicit.ro" \
    "" \
    "Hola E++
"

run_err external_tipo_fuera \
    "$root/tests/errors/external_tipo_fuera.ro" \
    "solo estan disponibles en firmas de funciones externas"

run_ok external_callback \
    "$root/tests/integration/external_callback.ro" \
    "" \
    "30
7
10
"

run_ok external_opaque \
    "$root/tests/integration/external_opaque.ro" \
    "" \
    "0
0
1
f no es null
"

run_ok external_string_owned \
    "$root/tests/integration/external_string_owned.ro" \
    "" \
    "hola estatico
hola estatico
hola E++
hola mundo
"

run_err external_callback_retorno \
    "$root/tests/errors/external_callback_retorno.ro" \
    "no puede devolver un callback"

run_err external_owned_invalido \
    "$root/tests/errors/external_owned_invalido.ro" \
    "owned y static solo se pueden usar con un retorno de tipo string"

run_err_flags external_abi_bad \
    "$root/tests/errors/external_abi_bad.ro" \
    "conflicting types" \
    --ffi-check

run_ok external_struct_punto \
    "$root/tests/integration/external_struct_punto.ro" \
    "" \
    "5.0
7
"

run_err external_struct_no_pod \
    "$root/tests/errors/external_struct_no_pod.ro" \
    "struct POD"

run_ok external_buffer \
    "$root/tests/integration/external_buffer.ro" \
    "" \
    "15
3.0
0
"

run_err external_buffer_retorno \
    "$root/tests/errors/external_buffer_retorno.ro" \
    "no puede devolver un buffer"

run_ok external_variadic \
    "$root/tests/integration/external_variadic.ro" \
    "" \
    "hola 1 2
done
"


echo ""
echo "=== Busqueda de modulos (-I) ==="

run_ok import_directorio_i \
    "$root/tests/integration/import_directorio_i.ro" \
    "" \
    "modulo via -I
" \
    "-I $root/tests/fixtures/modulos"

echo ""
echo "=== Manifiesto rivo.toml ==="

# Empezar sin artefactos de builds anteriores.
rm -rf \
    "$root/tests/fixtures/manifest/con_entrada/target" \
    "$root/tests/fixtures/manifest/con_release/target" \
    "$root/tests/fixtures/manifest/con_binario/target"

run_ok manifest_dependencias \
    "$root/tests/fixtures/manifest/con_dependencias/app.ro" \
    "" \
    "modulo via manifiesto
"

run_no_input_ok manifest_entrada \
    "$root/tests/fixtures/manifest/con_entrada" \
    "hola desde el manifiesto
"

run_ok manifest_compilador \
    "$root/tests/fixtures/manifest/con_compilador/programa.ro" \
    "" \
    "compilado con opciones del manifiesto
"

run_err manifest_mal_formado \
    "$root/tests/fixtures/manifest/mal_formado/programa.ro" \
    "clave fuera de una tabla"

run_err manifest_optimizar_invalido \
    "$root/tests/fixtures/manifest/optimizar_invalido/programa.ro" \
    "'optimizar' debe ser"

# Artefactos en target/: perfil, nombre del binario y
# caché por fecha.
run_no_input_bin_ok manifest_target_debug \
    "$root/tests/fixtures/manifest/con_entrada" \
    "target/debug/con_entrada" \
    "hola desde el manifiesto
"

run_no_input_bin_ok manifest_target_release \
    "$root/tests/fixtures/manifest/con_release" \
    "target/release/con_release" \
    "build release
"

run_no_input_bin_ok manifest_nombre_binario \
    "$root/tests/fixtures/manifest/con_binario" \
    "target/debug/app-especial" \
    "binario especial
"

run_no_input_cached manifest_cache \
    "$root/tests/fixtures/manifest/con_entrada"

# --run: compilar y ejecutar en una sola pasada.
if (cd "$root/tests/fixtures/manifest/con_entrada" \
        && "$rivo" --run) \
        > "$tmp/manifest_run.log" 2>&1 && \
   grep -q "hola desde el manifiesto" \
        "$tmp/manifest_run.log"; then
    echo "PASS  manifest_run"
    passes=$((passes + 1))
else
    echo "FAIL  manifest_run"
    sed 's/^/      /' "$tmp/manifest_run.log"
    fails=$((fails + 1))
fi

# Dejar el árbol limpio de artefactos.
rm -rf \
    "$root/tests/fixtures/manifest/con_entrada/target" \
    "$root/tests/fixtures/manifest/con_release/target" \
    "$root/tests/fixtures/manifest/con_binario/target"

echo ""
echo "=== Correcciones de fases ==="

run_ok division_entera \
    "$root/tests/integration/division_entera.ro" \
    "" \
    '3
-3
3
3.5
3.5
2
8
'
run_ok igualdad_cadenas \
    "$root/tests/integration/igualdad_cadenas.ro" \
    "" \
    'true
true
true
true
true
true
false
true
'
run_ok escapes \
    "$root/tests/integration/escapes.ro" \
    "" \
    '3
a
b
true
"hola"
6
uno\dos
7
3
'
run_ok comentario_inicio_bloque \
    "$root/tests/integration/comentario_inicio_bloque.ro" \
    "" \
    '10
si
'

run_ok list_concat \
    "$root/tests/integration/list_concat.ro" \
    "" \
    '3
1
3
2
2
5
3
3.5
3
c
'

run_err list_igualdad \
    "$root/tests/errors/list_igualdad.ro" \
    "invalid operation for these types"

run_err division_float \
    "$root/tests/errors/division_float.ro" \
    "tipo incompatible: se esperaba float pero se encontro int"

run_fail stdlib_json_invalido \
    "$root/tests/errors/stdlib_json_invalido.ro" \
    "no existe en el map"


echo ""
echo '=== Comentarios multi-linea (#! ... !#) ==='

run_ok comentario_multilinea \
    "$root/tests/integration/comentario_multilinea.ro" \
    "" \
    '1
3
#! no es comentario
tampoco !# lo es
'

run_err comentario_multilinea_sin_cerrar \
    "$root/tests/errors/comentario_multilinea_sin_cerrar.ro" \
    "comentario de multiple lineas sin cerrar"


echo ""
echo "=== f-strings ==="

run_ok fstring \
    "$root/tests/integration/fstring.ro" \
    "" \
    'hola Amine Benapuiss
20 19.99 true A
llaves { y } sin interpolacion
escape	tab y salto
en dos lineas
indice 8 y longitud 2
funcion 8
mapa valor Amine Benapuiss
asignado 20
aAmine BenapuissbAmine Benapuissc
[]
concat con 20
'

run_err fstring_tipo \
    "$root/tests/errors/fstring_tipo.ro" \
    "solo se pueden interpolar int, float, string, bool o char"

run_err fstring_sin_cerrar \
    "$root/tests/errors/fstring_sin_cerrar.ro" \
    "f-string sin cerrar"

run_err fstring_llave_suelta \
    "$root/tests/errors/fstring_llave_suelta.ro" \
    "llave de cierre sin abrir"


echo ""
echo "=== Continuación de línea entre paréntesis ==="

run_ok continuacion_linea \
    "$root/tests/integration/continuacion_linea.ro" \
    "" \
    "continuacion
"

run_err continuacion_linea_pendiente \
    "$root/tests/errors/continuacion_linea_pendiente.ro" \
    "parentesis sin cerrar"


echo ""
echo "=== Optimización (-O1 / -O2) ==="

run_ok hello_O1 \
    "$root/tests/basic/hello.ro" \
    "" \
    "Hola mundo
" \
    "-O1"

run_ok hello_O2 \
    "$root/tests/basic/hello.ro" \
    "" \
    "Hola mundo
" \
    "-O2"

run_ok fstring_O2 \
    "$root/tests/integration/fstring.ro" \
    "" \
    'hola Amine Benapuiss
20 19.99 true A
llaves { y } sin interpolacion
escape	tab y salto
en dos lineas
indice 8 y longitud 2
funcion 8
mapa valor Amine Benapuiss
asignado 20
aAmine BenapuissbAmine Benapuissc
[]
concat con 20
' \
    "-O2"


echo ""
echo "=== Biblioteca estandar ==="

run_ok stdlib_texto \
    "$root/tests/stdlib/stdlib_texto.ro" \
    "" \
    'HOLA E++
hola e++
espacio
true
false
ababab
3
0
Hola mundo
E++
edcba

a b c
3
defg
TUNel TUNel
true
true
true
3
tres
uno dos tres
'
run_ok stdlib_matematicas \
    "$root/tests/stdlib/stdlib_matematicas.ro" \
    "" \
    '5
5
2.5
3
7
10
0
true
true
-1
1024
120
6
12
3.0
212.0
0.0
3.0
256.0
0.0
1.0
0.0
2.0
1.0
true
true
3
2
3
true
false
'
run_ok stdlib_json \
    "$root/tests/stdlib/stdlib_json.ro" \
    "" \
    '{"a":"1"}
1
1
0
{}
{"k":"x,y"}
x,y
a\"b
a"b
uno
dos
barra\fin
tab\there
'
run_ok stdlib_ficheros \
    "$root/tests/stdlib/stdlib_ficheros.ro" \
    "" \
    'true
true
7
uno
dos
2
dos
true
a
b
false
-1
true
true
false
true
false
false
true
true
true
false
'
run_ok stdlib_tiempo_aleatorio \
    "$root/tests/stdlib/stdlib_tiempo_aleatorio.ro" \
    "" \
    'true
true
true
false
5
true
true
'
run_ok stdlib_fecha \
    "$root/tests/stdlib/stdlib_fecha.ro" \
    "" \
    '0
1970-01-01
2024-02-29
365
-1
2024-02-29
2025-01-01
2023-12-31
true
false
true
true
false
false
false
1
lunes
jueves
29
28
0
enero
diciembre

10
8
19
6
'
run_ok stdlib_sistema \
    "$root/tests/stdlib/stdlib_sistema.ro" \
    "" \
    'true
true
true
true
true
false
true
true
valor123
false
true
true
true
true
true
true
'
run_ok stdlib_estadistica \
    "$root/tests/stdlib/stdlib_estadistica.ro" \
    "" \
    '40.0
6.0
5.0
1.5
2.0
9.0
7.0
3.0
2.5
4.0
4.0
2.0
2.0
9.0
20.0
0.0
0.0
0.0
8
2.0
'
run_ok stdlib_validacion \
    "$root/tests/stdlib/stdlib_validacion.ro" \
    "" \
    'true
true
true
false
false
false
false
true
true
false
false
false
false
false
false
true
true
false
true
false
true
false
true
true
false
false
true
false
false
true
true
false
false
false
false
false
false
false
'
run_ok stdlib_csv \
    "$root/tests/stdlib/stdlib_csv.ro" \
    "" \
    '3
2
nombre
Perez, Ana
30
nombre,edad
"Perez, Ana",25
Juan,30
a,"b,c","d""e"
"hola"
"a,b"
"say ""hi"""
hola
say "hi"
hola
"uno
dos",tres
1
2
uno
dos
0
0
3

1
'
run_ok stdlib_mgl \
    "$root/tests/stdlib/stdlib_mgl.ro" \
    "" \
    '0
1
2
3
4
5
0
0


false
true
0
ok
' \
    "-l X11 -l Xft -l fontconfig"

run_ok stdlib_rutas \
    "$root/tests/stdlib/stdlib_rutas.ro" \
    "" \
    'usr/local
usr/local
/bin
/bin
a
rivo

rivo
usr/local
/usr
/

txt
gz



doc
sinpunto
nota
true
false
a/c
/a/b/c
/b
../b
.
.
a/b
3
a
b
c
'
run_ok stdlib_color \
    "$root/tests/stdlib/stdlib_color.ro" \
    "" \
    '13
hola
10
ab
10
x
9
x
hola
texto plano
9
10
9
9
juntos
16
x
15
x
22
x
18
x
r
r
'
run_ok stdlib_string \
    "$root/tests/stdlib/stdlib_string.ro" \
    "" \
    '0123456789
Hola Mundo Rivo
rIVO
0123456789abcdefABCDEF
'
run_ok stdlib_os \
    "$root/tests/stdlib/stdlib_os.ro" \
    "" \
    'true
true
true
true
ready
true
true
'
run_ok stdlib_pathlib \
    "$root/tests/stdlib/stdlib_pathlib.ro" \
    "" \
    'b.txt
b.txt
a
.gz
archivo.tar
2
'
run_ok stdlib_datetime \
    "$root/tests/stdlib/stdlib_datetime.ro" \
    "" \
    'true
2024-03-01
2
true
'
run_ok stdlib_platform \
    "$root/tests/stdlib/stdlib_platform.ro" \
    "" \
    'true
true
true
'
run_ok stdlib_base64 \
    "$root/tests/stdlib/stdlib_base64.ro" \
    "" \
    'Uml2bw==
Rivo
'
run_ok stdlib_html \
    "$root/tests/stdlib/stdlib_html.ro" \
    "" \
    '&lt;a&amp;&quot;&#x27;
true
'
run_ok stdlib_fnmatch \
    "$root/tests/stdlib/stdlib_fnmatch.ro" \
    "" \
    'true
true
false
false
'
run_ok stdlib_calendar \
    "$root/tests/stdlib/stdlib_calendar.ro" \
    "" \
    'true
29
enero
1
lunes
'
run_ok stdlib_shutil \
    "$root/tests/stdlib/stdlib_shutil.ro" \
    "" \
    'true
true
contenido
9
true
true
'

echo ""
echo "=== Formateador ==="

printf 'fn main():\n\tvar x:int = 1   \n\n\n\n\tprint(x)\n' \
    > "$tmp/fmt_limpia.ro"

if "$rivo" fmt "$tmp/fmt_limpia.ro" > "$tmp/fmt.log" 2>&1 &&
        "$rivo" fmt --check "$tmp/fmt_limpia.ro" >> "$tmp/fmt.log" 2>&1 &&
        "$rivo" "$tmp/fmt_limpia.ro" -o "$tmp/fmt_limpia.bin" >> "$tmp/fmt.log" 2>&1 &&
        [ "$("$tmp/fmt_limpia.bin")" = "1" ]; then
    echo "PASS  fmt_limpia"
    passes=$((passes + 1))
else
    echo "FAIL  fmt_limpia"
    sed 's/^/      /' "$tmp/fmt.log"
    fails=$((fails + 1))
fi

printf 'fn main():\n\tprint("hola")   \n' > "$tmp/fmt_check.ro"

if "$rivo" fmt --check "$tmp/fmt_check.ro" > "$tmp/fmt_check.log" 2>&1; then
    echo "FAIL  fmt_check (no detecto el formato pendiente)"
    fails=$((fails + 1))
else
    if grep -q "necesita formato" "$tmp/fmt_check.log"; then
        echo "PASS  fmt_check"
        passes=$((passes + 1))
    else
        echo "FAIL  fmt_check (mensaje inesperado)"
        sed 's/^/      /' "$tmp/fmt_check.log"
        fails=$((fails + 1))
    fi
fi


echo ""
echo "=== Avisos ==="

mkdir -p "$tmp/aviso"

cat > "$tmp/aviso/main.ro" <<'EOF'
fn main():
    var sin_usar:int = 1
    print("hola")
EOF

if "$rivo" "$tmp/aviso/main.ro" -o "$tmp/aviso/bin" \
        > "$tmp/aviso/log" 2>&1 &&
        grep -q "no se usa" "$tmp/aviso/log" &&
        [ -x "$tmp/aviso/bin" ]; then
    echo "PASS  aviso_variable_sin_usar"
    passes=$((passes + 1))
else
    echo "FAIL  aviso_variable_sin_usar"
    sed 's/^/      /' "$tmp/aviso/log"
    fails=$((fails + 1))
fi

cat > "$tmp/aviso/rivo.toml" <<'EOF'
[proyecto]
nombre = "aviso"
version = "0.1.0"
entrada = "main.ro"

[compilador]
advertencias_como_errores = "true"
EOF

if "$rivo" "$tmp/aviso/main.ro" -o "$tmp/aviso/bin2" \
        > "$tmp/aviso/log2" 2>&1; then
    echo "FAIL  aviso_como_error (compilo pese al aviso)"
    fails=$((fails + 1))
else
    echo "PASS  aviso_como_error"
    passes=$((passes + 1))
fi


echo ""
echo "=== Optimizacion (-O) ==="

cat > "$tmp/opt_prog.ro" <<'EOF'
fn fib(n:int) -> int:
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

fn main():
    var total:int = 0
    var i:int = 0
    while i < 10:
        total = total + fib(i)
        i = i + 1
    print(total)
EOF

for nivel in 0 1 2 3; do
    run_ok "opt_o$nivel" "$tmp/opt_prog.ro" "" "88
" "-O$nivel"
done


echo ""
echo "=== Cache de compilacion ==="

mkdir -p "$tmp/cache"

cat > "$tmp/cache/rivo.toml" <<'EOF'
[proyecto]
nombre = "cache"
version = "0.1.0"
EOF

cat > "$tmp/cache/mod.ro" <<'EOF'
fn valor() -> int:
    return 1
EOF

cat > "$tmp/cache/main.ro" <<'EOF'
import mod

fn main():
    print(mod.valor())
EOF

cache_run() {
    (cd "$tmp/cache" && "$rivo" main.ro --run "$@") > "$tmp/cache/log" 2>&1
}

# a) segundo build sin cambios -> cache
cache_run
cache_run
if grep -q "sin cambios" "$tmp/cache/log"; then
    echo "PASS  cache_sin_cambios"
    passes=$((passes + 1))
else
    echo "FAIL  cache_sin_cambios"
    sed 's/^/      /' "$tmp/cache/log"
    fails=$((fails + 1))
fi

# b) touch sin cambio real -> sigue fresco
touch "$tmp/cache/main.ro"
cache_run
if grep -q "sin cambios" "$tmp/cache/log"; then
    echo "PASS  cache_touch_sin_cambio"
    passes=$((passes + 1))
else
    echo "FAIL  cache_touch_sin_cambio"
    sed 's/^/      /' "$tmp/cache/log"
    fails=$((fails + 1))
fi

# c) cambiar -O dentro del mismo perfil -> recompila
cache_run -O2
cache_run -O3
if grep -q "Compilando" "$tmp/cache/log"; then
    echo "PASS  cache_invalida_por_flags"
    passes=$((passes + 1))
else
    echo "FAIL  cache_invalida_por_flags"
    sed 's/^/      /' "$tmp/cache/log"
    fails=$((fails + 1))
fi

# d) editar un modulo dependiente -> recompila
cat > "$tmp/cache/mod.ro" <<'EOF'
fn valor() -> int:
    return 2
EOF
cache_run
if grep -q "Compilando" "$tmp/cache/log" &&
        grep -q "2" "$tmp/cache/log"; then
    echo "PASS  cache_invalida_por_modulo"
    passes=$((passes + 1))
else
    echo "FAIL  cache_invalida_por_modulo"
    sed 's/^/      /' "$tmp/cache/log"
    fails=$((fails + 1))
fi


echo ""
echo "=== Bloqueados conocidos ==="

run_ok input_variable \
    "$root/tests/integration/input_variable.ro" \
    "Ana" \
    "Ana
"


echo ""
echo "----------------------------------------"
echo "TESTS: $passes pasan, $fails fallan"

if [ "$fails" -ne 0 ]; then
    exit 1
fi

echo "TESTS: PASS"
