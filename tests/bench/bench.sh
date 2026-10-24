#!/bin/sh
#
# Benchmarks de E++ / Rivo.
#
# Mide las metricas base de v0.8 (plan_v08.md, Fase 0):
#   1. arranque del CLI (--version)
#   2. compilacion en frio de hello world
#   3. ejecucion con cache fresca
#   4. compilacion del corpus (sin manifiesto, sin cache)
#
# Uso:
#     sh tests/bench/bench.sh
#
# Requiere build/rivo ya compilado. Usa hyperfine si esta
# disponible; si no, mide con date(1) (GNU) y, en su defecto,
# avisa y no hace nada.

set -u

root=$(cd "$(dirname "$0")/../.." && pwd)
build="${RIVO_BUILD:-$root/build}"
rivo="$build/rivo"

if [ ! -x "$rivo" ]; then
    echo "No se encuentra $rivo. Ejecute antes: make build" >&2
    exit 1
fi

runs=${BENCH_RUNS:-10}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Proyecto minimo con manifiesto (necesario para la cache).
mkdir -p "$tmp/app"
cat > "$tmp/app/rivo.toml" <<'EOF'
[proyecto]
nombre = "bench"
version = "0.1.0"
EOF
cat > "$tmp/app/hello.ro" <<'EOF'
fn main():
    print("Hola mundo")
EOF

# Corpus sin manifiesto: compilacion directa, sin cache.
corpus=""
for f in "$root"/tests/basic/*.ro "$root"/tests/integration/*.ro; do
    corpus="$corpus $f"
done

have_date_ns() {
    date +%s%N 2>/dev/null | grep -qE '^[0-9]{16,}$'
}

# timeit <n> <cmd...> -> imprime "media_ms min_ms"
timeit() {
    n=$1
    shift

    if command -v hyperfine >/dev/null 2>&1; then
        out=$(hyperfine --warmup 2 --runs "$n" --style basic \
            "$*" 2>/dev/null | tr -d '\r')
        mean=$(printf '%s\n' "$out" |
            sed -n 's/.*[Mm]ean.*[[:space:]]\([0-9.]*\) *ms.*/\1/p' |
            head -1)
        min=$(printf '%s\n' "$out" |
            sed -n 's/.*[Mm]in.*[[:space:]]\([0-9.]*\) *ms.*/\1/p' |
            head -1)
        if [ -n "$mean" ]; then
            printf '%s %s\n' "${mean:-0}" "${min:-0}"
            return
        fi
    fi

    if have_date_ns; then
        total=0
        best=""
        i=0
        while [ "$i" -lt "$n" ]; do
            start=$(date +%s%N)
            eval "$*" >/dev/null 2>&1
            end=$(date +%s%N)
            ms=$(( (end - start) / 1000000 ))
            total=$((total + ms))
            if [ -z "$best" ] || [ "$ms" -lt "$best" ]; then
                best=$ms
            fi
            i=$((i + 1))
        done
        printf '%s %s\n' "$((total / n))" "$best"
        return
    fi

    printf '0 0\n'
}

row() {
    name=$1
    shift
    set -- $(timeit "$runs" "$@")
    mean=${1:-0}
    min=${2:-0}
    printf '%-34s %10s ms %10s ms\n' "$name" "$mean" "$min"
}

printf '\n== Benchmarks E++ v0.8 ==\n'
printf 'rivo: %s\n' "$("$rivo" --version 2>&1 | head -1)"
printf 'hyperfine: %s\n' "$(command -v hyperfine >/dev/null 2>&1 && echo si || echo no)"
printf 'runs: %s\n\n' "$runs"
printf '%-34s %13s %13s\n' 'metrica' 'media' 'mejor'
printf '%s\n' '------------------------------------------------------------------'

# 1. Arranque del CLI.
row "arranque (--version)" "$rivo --version"

# 2. Compilacion en frio (sin cache).
row "compilar hello (frio)" "rm -rf '$tmp/app/target' && (cd '$tmp/app' && '$rivo' hello.ro --run)"

# 3. Cache fresca: primer build, luego solo ejecutar.
(cd "$tmp/app" && "$rivo" hello.ro --run >/dev/null 2>&1)
row "hello con cache fresca" "(cd '$tmp/app' && '$rivo' hello.ro --run)"

# 4. Corpus sin manifiesto (sin cache): un binario por archivo.
compile_corpus() {
    for f in $corpus; do
        "$rivo" "$f" -o "$tmp/corpus.bin" >/dev/null 2>&1
    done
}
row "compilar corpus (sin cache)" compile_corpus

printf '%s\n' '------------------------------------------------------------------'
printf 'Nota: "mejor" es el minimo observado; util para detectar picos.\n\n'
