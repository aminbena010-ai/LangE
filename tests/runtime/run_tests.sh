#!/bin/sh
#
# Tests directos del runtime de E++.
#
# Compila src/runtime/*.c + tests/runtime/test_runtime.c con
# AddressSanitizer + UndefinedBehaviorSanitizer, ejecuta el
# binario y comprueba la salida estándar.
#
# Uso:
#     sh tests/runtime/run_tests.sh

set -e

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../.." && pwd)
bin="$dir/test_runtime.bin"

CC="${CC:-cc}"

"$CC" -std=c11 -Wall -Wextra -g \
    -fsanitize=address,undefined \
    -I"$root/include" \
    "$dir/test_runtime.c" "$root"/src/runtime/*.c \
    -lm \
    -o "$bin"

"$bin" < "$dir/input.txt" > "$dir/actual_stdout.txt"

if ! diff -u "$dir/expected_stdout.txt" "$dir/actual_stdout.txt"; then
    echo "RUNTIME TESTS: FALLO (salida inesperada)"
    exit 1
fi

rm -f "$dir/actual_stdout.txt" "$bin"
echo "RUNTIME TESTS: PASS"
