#!/usr/bin/env bash

set -Eeuo pipefail

die() {
    printf 'install.sh: %s\n' "$*" >&2
    exit 1
}

has_llvm_config() {
    command -v llvm-config >/dev/null 2>&1 ||
        command -v llvm-config-22 >/dev/null 2>&1 ||
        command -v llvm-config-21 >/dev/null 2>&1 ||
        command -v llvm-config-20 >/dev/null 2>&1 ||
        command -v llvm-config-19 >/dev/null 2>&1 ||
        command -v llvm-config-18 >/dev/null 2>&1 ||
        command -v llvm-config-17 >/dev/null 2>&1
}

run_sudo() {
    if (( EUID == 0 )); then
        "$@"
    else
        sudo "$@"
    fi
}

if (( EUID == 0 )) && [[ -n ${SUDO_USER:-} && $SUDO_USER != root ]]; then
    die "no ejecute install.sh con sudo; el script solo usa sudo para instalar dependencias. Ejecute: su - $SUDO_USER -c '$PWD/install.sh'"
fi

ensure_writable_dir() {
    local dir=$1 owner
    mkdir -p -- "$dir"
    if [[ -w $dir && -w $dir/. ]]; then
        return 0
    fi
    owner=$(stat -c %U -- "$dir" 2>/dev/null || stat -f %Su -- "$dir")
    printf 'Corrigiendo propietario de %s (actual: %s).\n' "$dir" "$owner" >&2
    run_sudo chown -R "$(id -un):$(id -gn)" -- "$dir"
}

usage() {
    cat <<'EOF'
Uso: ./install.sh [opciones]

Opciones:
  -y, --no-prompt   Instalación sin cuestionario (valores por defecto)
  -h, --help        Mostrar esta ayuda

En terminal, el script muestra un cuestionario breve (respuestas
1/2/3) para elegir el snapshot de versión, el destino de instalación
y las pruebas, y confirma la versión que se instalará en el
directorio del usuario. Sin terminal (o con -y) se instala
directamente con los valores por defecto (pruebas completas); en
ambos casos se muestra el progreso del proyecto (tareas
resueltas/pendientes).

Variables de entorno: PREFIX (destino), RIVO_BUILD_DIR,
RIVO_DIST_DIR, RIVO_NO_PROMPT=1 (igual que -y).
EOF
}

# Pide un número en el rango dado. $1=variable $2=texto
# $3=valor por defecto (Intro) $4=mínimo $5=máximo.
ask_choice() {
    local __var=$1 __prompt=$2 __def=$3 __min=$4 __max=$5 __ans
    while true; do
        __ans=""
        read -r -p "$__prompt [$__def]: " __ans || __ans=""
        if [[ -z $__ans ]]; then
            __ans=$__def
        fi
        if [[ $__ans =~ ^[0-9]+$ ]] &&
            (( 10#$__ans >= __min && 10#$__ans <= __max )); then
            printf -v "$__var" '%d' "$((10#$__ans))"
            return 0
        fi
        printf 'Opción no válida: escriba un número del %s al %s.\n' \
            "$__min" "$__max" >&2
    done
}

# Siguiente snapshot de la convención YYfaN (26fa1 -> 26fa2).
snapshot_next() {
    local snap=$1
    if [[ $snap =~ ^([0-9]{2}fa)([0-9]+)$ ]]; then
        printf '%s%d' "${BASH_REMATCH[1]}" "$(( 10#${BASH_REMATCH[2]} + 1 ))"
    else
        printf '%s' "$snap"
    fi
}

snapshot_set() {
    local snap=$1 file="$script_dir/include/version.h"
    if [[ $os == Darwin ]]; then
        sed -i '' \
            -e "s/^#define RIVO_SNAPSHOT \".*\"$/#define RIVO_SNAPSHOT \"$snap\"/" \
            "$file"
    else
        sed -i \
            -e "s/^#define RIVO_SNAPSHOT \".*\"$/#define RIVO_SNAPSHOT \"$snap\"/" \
            "$file"
    fi
}

# Ejecuta las pruebas elegidas en el cuestionario (tras compilar).
run_requested_tests() {
    local status=0
    case $run_tests in
        full)
            printf '\nEjecutando pruebas del compilador...\n'
            RIVO_BUILD="$build_dir" \
                sh "$script_dir/tests/run_tests.sh" || status=1
            printf 'Ejecutando pruebas del runtime...\n'
            sh "$script_dir/tests/runtime/run_tests.sh" || status=1
            ;;
        runtime)
            printf '\nEjecutando pruebas del runtime...\n'
            sh "$script_dir/tests/runtime/run_tests.sh" || status=1
            ;;
        *)
            return 0
            ;;
    esac
    if (( status != 0 )); then
        die "las pruebas fallaron; instalación cancelada."
    fi
}

no_prompt=0
for argument in "$@"; do
    case "$argument" in
        -y|--no-prompt) no_prompt=1 ;;
        -h|--help) usage; exit 0 ;;
        *) die "opción desconocida: '$argument' (use --help)" ;;
    esac
done

os=$(uname -s)
case "$os" in
    Linux|Darwin) ;;
    MINGW*|MSYS*|CYGWIN*) die "en Windows, ejecute install.ps1 desde PowerShell." ;;
    *) die "sistema operativo no soportado: $os" ;;
esac

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# ---- Versión y progreso del proyecto ----

code_version=$(sed -n 's/^#define RIVO_VERSION "\(.*\)"$/\1/p' \
    "$script_dir/include/version.h")
code_snapshot=$(sed -n 's/^#define RIVO_SNAPSHOT "\(.*\)"$/\1/p' \
    "$script_dir/include/version.h")

prefix=${PREFIX:-"$HOME/.local"}
[[ "$prefix" == /* ]] || die "PREFIX debe ser una ruta absoluta."

installed_version="no instalada"
if [[ -x $prefix/bin/rivo ]]; then
    installed_version=$("$prefix/bin/rivo" --version 2>/dev/null |
        awk 'NR == 1 { print $2 }') || installed_version="desconocida"
elif command -v rivo >/dev/null 2>&1; then
    installed_version=$(rivo --version 2>/dev/null |
        awk 'NR == 1 { print $2 }') || installed_version="desconocida"
fi
[[ -n $installed_version ]] || installed_version="no instalada"

todo_file="$script_dir/document/TODO.md"
todo_done=0
todo_pending=0
if [[ -f $todo_file ]]; then
    todo_done=$(grep -c '\[x\]' "$todo_file" || true)
    todo_pending=$(grep -c '\[ \]' "$todo_file" || true)
    todo_done=${todo_done:-0}
    todo_pending=${todo_pending:-0}
fi
todo_total=$((todo_done + todo_pending))
todo_percent=0
if (( todo_total > 0 )); then
    todo_percent=$((todo_done * 100 / todo_total))
fi

printf '=== Instalador de Rivo ===\n'
printf 'Versión en el código:  %s+%s\n' "$code_version" "$code_snapshot"
printf 'Versión instalada:     %s\n' "$installed_version"
printf 'Progreso:              %s de %s tareas resueltas (%s%%)\n' \
    "$todo_done" "$todo_total" "$todo_percent"

# ---- Cuestionario (solo terminal, sin -y ni RIVO_NO_PROMPT) ----

new_snapshot=$code_snapshot
run_tests=full

if (( no_prompt == 0 )) && [[ -z ${RIVO_NO_PROMPT:-} ]] &&
    [[ -t 0 && -t 1 ]]; then

    printf '\nCuestionario (Intro = valor entre corchetes):\n'

    snapshot_next_value=$(snapshot_next "$code_snapshot")

    printf '\n1) Snapshot de versión\n'
    printf '   1) Mantener      %s+%s\n' "$code_version" "$code_snapshot"
    printf '   2) Siguiente     %s+%s\n' "$code_version" "$snapshot_next_value"
    printf '   3) Personalizado\n'
    ask_choice snapshot_choice '   Respuesta' 1 1 3
    case $snapshot_choice in
        2) new_snapshot=$snapshot_next_value ;;
        3)
            read -r -p '   Nuevo snapshot: ' custom_snapshot ||
                custom_snapshot=""
            [[ -n $custom_snapshot ]] || die "snapshot vacío."
            [[ $custom_snapshot =~ ^[A-Za-z0-9][A-Za-z0-9.+-]*$ ]] ||
                die "snapshot no válido: use letras, dígitos, '.', '+' o '-'."
            new_snapshot=$custom_snapshot
            ;;
    esac

    printf '\n2) Destino de instalación\n'
    printf '   1) %s\n' "$prefix"
    printf '   2) Otra ruta absoluta\n'
    ask_choice prefix_choice '   Respuesta' 1 1 2
    if (( prefix_choice == 2 )); then
        read -r -p '   Ruta absoluta: ' custom_prefix || custom_prefix=""
        [[ $custom_prefix == /* ]] ||
            die "la ruta debe ser absoluta (empezar por /)."
        prefix=$custom_prefix
    fi

    printf '\n3) Pruebas antes de instalar\n'
    printf '   1) Completa (compilador + runtime)\n'
    printf '   2) Solo runtime\n'
    printf '   3) Ninguna\n'
    ask_choice tests_choice '   Respuesta' 1 1 3
    case $tests_choice in
        1) run_tests=full ;;
        2) run_tests=runtime ;;
        3) run_tests=none ;;
    esac

    printf '\n4) Confirmación\n'
    printf '   Se instalará E++ %s+%s en %s\n' \
        "$code_version" "$new_snapshot" "$prefix"
    printf '   1) Sí, instalar\n'
    printf '   2) No, cancelar\n'
    ask_choice confirm '   Respuesta' 1 1 2
    if (( confirm == 2 )); then
        printf 'Instalación cancelada.\n'
        exit 0
    fi
    printf '\n'
fi

if [[ $new_snapshot != "$code_snapshot" ]]; then
    snapshot_set "$new_snapshot"
    printf 'Snapshot actualizado en include/version.h: %s -> %s\n' \
        "$code_snapshot" "$new_snapshot"
    code_snapshot=$new_snapshot
fi

install_dependencies() {
    if [[ "$os" == Darwin ]]; then
        command -v brew >/dev/null 2>&1 ||
            die "instale Homebrew (https://brew.sh) y vuelva a ejecutar este script."
        if ! command -v cmake >/dev/null 2>&1 ||
            ! brew list --versions llvm >/dev/null 2>&1; then
            brew install cmake llvm
        fi
        return
    fi

    if command -v cmake >/dev/null 2>&1 &&
        command -v clang >/dev/null 2>&1 &&
        has_llvm_config &&
        command -v gpg >/dev/null 2>&1 &&
        command -v tar >/dev/null 2>&1 &&
        command -v sha256sum >/dev/null 2>&1 &&
        command -v awk >/dev/null 2>&1 &&
        command -v tr >/dev/null 2>&1; then
        return
    fi

    if (( EUID != 0 )) && ! command -v sudo >/dev/null 2>&1; then
        die "faltan dependencias y no se encontró sudo para instalarlas."
    fi

    if command -v apt-get >/dev/null 2>&1; then
        run_sudo apt-get update
        run_sudo apt-get install -y cmake clang llvm-dev build-essential gnupg
    elif command -v dnf >/dev/null 2>&1; then
        run_sudo dnf install -y cmake clang llvm-devel gcc gcc-c++ make gnupg2
    elif command -v pacman >/dev/null 2>&1; then
        run_sudo pacman -S --needed --noconfirm cmake clang llvm base-devel gnupg
    else
        die "instale CMake, LLVM, Clang y GPG con el gestor de paquetes de su distribución."
    fi
}

install_dependencies

for command in cmake clang tar awk tr; do
    command -v "$command" >/dev/null 2>&1 ||
        die "falta el requisito '$command'."
done

architecture=$(uname -m)
if [[ "$os" == Linux ]]; then
    command -v gpg >/dev/null 2>&1 ||
        die "falta GPG, necesario para firmar el paquete Linux."
    command -v sha256sum >/dev/null 2>&1 ||
        die "falta sha256sum, necesario para generar los checksums Linux."
    case "$architecture" in
        x86_64|aarch64) ;;
        *) die "arquitectura Linux no soportada: $architecture" ;;
    esac
else
    case "$architecture" in
        x86_64|arm64) ;;
        *) die "arquitectura macOS no soportada: $architecture" ;;
    esac
fi

if [[ "$os" == Darwin ]]; then
    default_build_dir="$script_dir/build/macos"
else
    default_build_dir="$script_dir/build"
fi
build_dir=${RIVO_BUILD_DIR:-"$default_build_dir"}
dist_dir=${RIVO_DIST_DIR:-"$script_dir/dist"}

ensure_writable_dir "$build_dir"
ensure_writable_dir "$dist_dir"

if [[ "$os" == Darwin ]]; then
    llvm_prefix=$(brew --prefix llvm)
    export PATH="$llvm_prefix/bin:$PATH"
    cmake -S "$script_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DLLVM_DIR="$llvm_prefix/lib/cmake/llvm"
    cmake --build "$build_dir" --parallel
    run_requested_tests
    cmake --install "$build_dir" --prefix "$prefix"
    printf '\nInstalado Rivo en %s\n' "$prefix"
    printf 'Ejecutable: %s/bin/rivo\n' "$prefix"
    printf 'Progreso del proyecto: %s de %s tareas resueltas (%s%%).\n' \
        "$todo_done" "$todo_total" "$todo_percent"
    case ":${PATH}:" in
        *":$prefix/bin:"*) ;;
        *) printf 'Añada %s/bin a PATH para ejecutar `rivo` desde cualquier directorio.\n' "$prefix" ;;
    esac
    exit 0
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/rivo-install.XXXXXXXX")
cleanup() {
    rm -rf -- "$tmp_dir"
}
trap cleanup EXIT

cmake -S "$script_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build_dir" --parallel

binary="$build_dir/rivo"
[[ -x "$binary" ]] || die "la compilación no generó $binary."
run_requested_tests
version=$("$binary" --version | awk 'NR == 1 { print $2 }')
[[ -n "$version" ]] || die "no se pudo obtener la versión de rivo."
safe_version=$(printf '%s' "$version" | tr -c 'A-Za-z0-9.+-' '_')

key_uid="Rivo Linux local release signing key"
if [[ -n ${RIVO_SIGNING_KEY_ID:-} ]]; then
    signing_key=$RIVO_SIGNING_KEY_ID
else
    mapfile -t signing_keys < <(
        gpg --batch --with-colons --list-secret-keys 2>/dev/null |
            awk -F: '$1 == "sec" && $5 != "" { print $5 }'
    )

    if (( ${#signing_keys[@]} == 0 )); then
        [[ -t 0 && -t 1 ]] ||
            die "no hay clave GPG. Ejecute desde una terminal para crear la clave local."
        printf 'No hay claves privadas GPG; se creará una clave local Ed25519.\n'
        printf 'GPG solicitará una frase de contraseña mediante pinentry.\n'
        GPG_TTY=$(tty 2>/dev/null || true)
        export GPG_TTY
        if ! gpg --quick-generate-key "$key_uid" ed25519 sign 0; then
            printf 'pinentry falló (terminal demasiado pequeña); se creará la clave sin frase de contraseña.\n' >&2
            gpgconf --kill gpg-agent 2>/dev/null || true
            gpg --batch --yes --pinentry-mode loopback --passphrase '' \
                --quick-generate-key "$key_uid" ed25519 sign 0 ||
                die "no se pudo crear la clave GPG local."
        fi
        mapfile -t signing_keys < <(
            gpg --batch --with-colons --list-secret-keys "$key_uid" 2>/dev/null |
                awk -F: '$1 == "sec" && $5 != "" { print $5 }'
        )
    fi

    if (( ${#signing_keys[@]} == 1 )); then
        signing_key=${signing_keys[0]}
    elif (( ${#signing_keys[@]} > 1 )); then
        [[ -t 0 && -t 1 ]] ||
            die "hay varias claves GPG; defina RIVO_SIGNING_KEY_ID con el key ID deseado."
        printf 'Seleccione la clave GPG que firmará el paquete:\n'
        for index in "${!signing_keys[@]}"; do
            printf '  %d) %s\n' "$((index + 1))" "${signing_keys[index]}"
        done
        read -r -p 'Número de clave: ' selection
        [[ $selection =~ ^[0-9]+$ ]] ||
            die "selección de clave inválida."
        (( selection >= 1 && selection <= ${#signing_keys[@]} )) ||
            die "selección de clave fuera de rango."
        signing_key=${signing_keys[selection - 1]}
    else
        die "no se encontró la clave GPG creada."
    fi
fi

gpg --batch --with-colons --list-secret-keys "$signing_key" |
    awk -F: '$1 == "sec" { found = 1 } END { exit !found }' ||
    die "no se puede acceder a la clave privada '$signing_key'."

archive_name="rivo-linux-${architecture}-${safe_version}.tar.gz"
archive="$dist_dir/$archive_name"
signature="$archive.asc"
stage="$tmp_dir/stage"
cmake --install "$build_dir" --prefix "$stage/usr"
mkdir -p -- "$stage/usr/share/doc/rivo"
cat > "$stage/usr/share/doc/rivo/INSTALL.txt" <<EOF
Rivo $version para Linux ($architecture)

Requisitos del sistema:
  - biblioteca compartida de LLVM compatible con la versión usada al compilar
  - clang, necesario para enlazar programas Rivo generados

Instalación en el directorio del usuario:
  mkdir -p "\$HOME/.local"
  tar -xzf $archive_name -C "\$HOME/.local"

Antes de extraer un paquete recibido de otra persona:
  gpg --import rivo-signing-key.asc
  gpg --verify $archive_name.asc $archive_name

Compruebe el fingerprint de la clave por un canal confiable independiente.
EOF

tar -czf "$archive" -C "$stage/usr" bin lib share
gpg --batch --yes --armor --detach-sign \
    --local-user "$signing_key" \
    --output "$signature" "$archive"
gpg --verify "$signature" "$archive"

public_key="$dist_dir/rivo-signing-key.asc"
gpg --armor --export "$signing_key" > "$public_key"
[[ -s "$public_key" ]] || die "no se pudo exportar la clave pública."
(
    cd -- "$dist_dir"
    sha256sum "$archive_name" "$(basename -- "$signature")" > SHA256SUMS
)

extract_dir="$tmp_dir/extracted"
mkdir -p -- "$extract_dir"
tar -xzf "$archive" -C "$extract_dir"
[[ -x "$extract_dir/bin/rivo" ]] ||
    die "el paquete firmado no contiene bin/rivo."
[[ -f "$extract_dir/lib/librivo_runtime.a" ]] ||
    die "el paquete firmado no contiene el runtime."
[[ -d "$extract_dir/share/rivo/lib" ]] ||
    die "el paquete firmado no contiene la biblioteca estándar."
[[ -f "$extract_dir/share/doc/rivo/INSTALL.txt" ]] ||
    die "el paquete firmado no contiene las instrucciones de instalación."

mkdir -p -- \
    "$prefix/bin" \
    "$prefix/lib" \
    "$prefix/share/rivo/lib" \
    "$prefix/share/doc/rivo"
install -m 0755 "$extract_dir/bin/rivo" "$prefix/bin/rivo"
install -m 0644 \
    "$extract_dir/lib/librivo_runtime.a" \
    "$prefix/lib/librivo_runtime.a"
cp -a "$extract_dir/share/rivo/lib/." "$prefix/share/rivo/lib/"
install -m 0644 \
    "$extract_dir/share/doc/rivo/INSTALL.txt" \
    "$prefix/share/doc/rivo/INSTALL.txt"

printf '\nInstalado Rivo %s en %s\n' "$version" "$prefix"
printf 'Ejecutable: %s/bin/rivo\n' "$prefix"
printf 'Progreso del proyecto: %s de %s tareas resueltas (%s%%).\n' \
    "$todo_done" "$todo_total" "$todo_percent"
printf 'Paquete: %s\n' "$archive"
printf 'Firma: %s\n' "$signature"
printf 'Clave pública: %s\n' "$public_key"
printf 'Checksums: %s/SHA256SUMS\n' "$dist_dir"
gpg --fingerprint "$signing_key"
printf 'Compruebe el fingerprint de la clave por un canal confiable antes de distribuirla.\n'
case ":${PATH}:" in
    *":$prefix/bin:"*) ;;
    *) printf 'Añada %s/bin a PATH para ejecutar `rivo` desde cualquier directorio.\n' "$prefix" ;;
esac
