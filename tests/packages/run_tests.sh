#!/bin/sh
#
# Tests del gestor de paquetes (v0.5 local + v0.6 registro Git):
# install, tree, list, add, remove, git, indice, search/info
# y publish -> install.
#
# Uso:
#     sh tests/packages/run_tests.sh

set -u

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../.." && pwd)
build="${RIVO_BUILD:-$root/build}"
rivo="$build/rivo"
tmp=$(mktemp -d)

trap 'rm -rf "$tmp"' EXIT

fails=0

fail() {
    echo "FAIL  $1"
    fails=$((fails + 1))
}

pass() {
    echo "PASS  $1"
}

if [ ! -x "$rivo" ]; then
    echo "No se encuentra $rivo. Ejecute antes:" >&2
    echo "  cmake -S . -B build && cmake --build build" >&2
    exit 1
fi

# -------------------------------------------------------
# 1. install + lock + compilacion con dependencia local
# -------------------------------------------------------

cp -R "$dir/fixtures/app" "$tmp/app"
cp -R "$dir/fixtures/libs" "$tmp/libs"

if (cd "$tmp/app" && "$rivo" install) > "$tmp/install.log" 2>&1; then
    pass "install_local"
else
    fail "install_local"
    sed 's/^/      /' "$tmp/install.log"
fi

if [ -f "$tmp/app/rivo.lock" ] &&
   grep -q 'nombre = "saludo"' "$tmp/app/rivo.lock" &&
   grep -q 'version = "0.1.0"' "$tmp/app/rivo.lock"; then
    pass "lock_local"
else
    fail "lock_local"
    cat "$tmp/app/rivo.lock" 2>/dev/null | sed 's/^/      /'
fi

if (cd "$tmp/app" && "$rivo" tree) > "$tmp/tree.log" 2>&1 &&
   grep -q 'app_paquetes 0.1.0' "$tmp/tree.log" &&
   grep -q 'saludo 0.1.0' "$tmp/tree.log"; then
    pass "tree_local"
else
    fail "tree_local"
    sed 's/^/      /' "$tmp/tree.log"
fi

if (cd "$tmp/app" && "$rivo" list) > "$tmp/list.log" 2>&1 &&
   grep -q 'saludo 0.1.0' "$tmp/list.log"; then
    pass "list_local"
else
    fail "list_local"
    sed 's/^/      /' "$tmp/list.log"
fi

if (cd "$tmp/app" && "$rivo" main.ro --run) > "$tmp/run.log" 2>&1 &&
   grep -q 'hola paquete' "$tmp/run.log"; then
    pass "build_con_lock"
else
    fail "build_con_lock"
    sed 's/^/      /' "$tmp/run.log"
fi

if (cd "$tmp" && "$rivo" app/main.ro --run) > "$tmp/parent_run.log" 2>&1 &&
   grep -q 'hola paquete' "$tmp/parent_run.log"; then
    pass "build_desde_padre"
else
    fail "build_desde_padre"
    sed 's/^/      /' "$tmp/parent_run.log"
fi

# -------------------------------------------------------
# 1b. tree recursivo (tres niveles)
# -------------------------------------------------------

mkdir -p "$tmp/nest/app" "$tmp/nest/b" "$tmp/nest/c"

cat > "$tmp/nest/app/rivo.toml" <<'EOF'
[proyecto]
nombre = "app_nido"
version = "0.1.0"

[dependencias]
b = "../b"
EOF

cat > "$tmp/nest/b/rivo.toml" <<'EOF'
[proyecto]
nombre = "b"
version = "0.1.0"

[dependencias]
c = "../c"
EOF

cat > "$tmp/nest/c/rivo.toml" <<'EOF'
[proyecto]
nombre = "c"
version = "0.2.0"
EOF

if (cd "$tmp/nest/app" && "$rivo" install) > "$tmp/nest_install.log" 2>&1 &&
   (cd "$tmp/nest/app" && "$rivo" tree) > "$tmp/nest_tree.log" 2>&1 &&
   grep -q '|-- b 0.1.0' "$tmp/nest_tree.log" &&
   grep -q '`-- c 0.2.0' "$tmp/nest_tree.log"; then
    pass "tree_recursivo"
else
    fail "tree_recursivo"
    sed 's/^/      /' "$tmp/nest_install.log"
    sed 's/^/      /' "$tmp/nest_tree.log" 2>/dev/null
fi

# -------------------------------------------------------
# 2. add version + remove (paquete en .rivo/deps)
# -------------------------------------------------------

mkdir -p "$tmp/ver/.rivo/deps"
cp -R "$dir/fixtures/libs/saludo" "$tmp/ver/.rivo/deps/saludo"

cat > "$tmp/ver/rivo.toml" <<'EOF'
[proyecto]
nombre = "app_version"
version = "0.1.0"
entrada = "main.ro"
EOF

cp "$dir/fixtures/app/main.ro" "$tmp/ver/main.ro"

if (cd "$tmp/ver" && "$rivo" add saludo@^0.1.0) > "$tmp/add.log" 2>&1 &&
   grep -q 'saludo = "\^0.1.0"' "$tmp/ver/rivo.toml" &&
   grep -q 'version = "0.1.0"' "$tmp/ver/rivo.lock"; then
    pass "add_version"
else
    fail "add_version"
    sed 's/^/      /' "$tmp/add.log"
    cat "$tmp/ver/rivo.toml" 2>/dev/null | sed 's/^/      /'
fi

if (cd "$tmp/ver" && "$rivo" main.ro --run) > "$tmp/ver_run.log" 2>&1 &&
   grep -q 'hola paquete' "$tmp/ver_run.log"; then
    pass "build_version"
else
    fail "build_version"
    sed 's/^/      /' "$tmp/ver_run.log"
fi

if (cd "$tmp/ver" && "$rivo" remove saludo) > "$tmp/remove.log" 2>&1 &&
   ! grep -q 'saludo = ' "$tmp/ver/rivo.toml" &&
   ! grep -q 'nombre = "saludo"' "$tmp/ver/rivo.lock"; then
    pass "remove_version"
else
    fail "remove_version"
    sed 's/^/      /' "$tmp/remove.log"
fi

# -------------------------------------------------------
# 3. version no disponible: error claro
# -------------------------------------------------------

mkdir -p "$tmp/bad/.rivo/deps"
cp -R "$dir/fixtures/libs/saludo" "$tmp/bad/.rivo/deps/saludo"

cat > "$tmp/bad/rivo.toml" <<'EOF'
[proyecto]
nombre = "app_mala"
version = "0.1.0"

[dependencias]
saludo = "^9.0.0"
EOF

if (cd "$tmp/bad" && "$rivo" install) > "$tmp/bad.log" 2>&1; then
    fail "version_no_disponible (deberia fallar)"
else
    if grep -q "no se encontro 'saludo'" "$tmp/bad.log"; then
        pass "version_no_disponible"
    else
        fail "version_no_disponible (mensaje inesperado)"
        sed 's/^/      /' "$tmp/bad.log"
    fi
fi

# -------------------------------------------------------
# 4. dependencias git y registro estatico
# -------------------------------------------------------

if ! command -v git >/dev/null 2>&1; then
    echo "SKIP  git no disponible"
else
    git init -q "$tmp/gitpkg"
    cp -R "$dir/fixtures/libs/saludo/." "$tmp/gitpkg/"
    git -C "$tmp/gitpkg" config user.email tests@rivo
    git -C "$tmp/gitpkg" config user.name tests
    git -C "$tmp/gitpkg" add .
    git -C "$tmp/gitpkg" commit -qm inicial
    git -C "$tmp/gitpkg" tag v0.1.0

    mkdir -p "$tmp/gitapp"
    cp "$dir/fixtures/app/main.ro" "$tmp/gitapp/main.ro"

    cat > "$tmp/gitapp/rivo.toml" <<EOF
[proyecto]
nombre = "app_git"
version = "0.1.0"
entrada = "main.ro"

[dependencias]
saludo = { git = "file://$tmp/gitpkg", tag = "v0.1.0" }
EOF

    if (cd "$tmp/gitapp" && "$rivo" install) > "$tmp/git_install.log" 2>&1 &&
       grep -q 'origen = "ruta:./.rivo/deps/saludo"' "$tmp/gitapp/rivo.lock" &&
       (cd "$tmp/gitapp" && "$rivo" main.ro --run) > "$tmp/git_run.log" 2>&1 &&
       grep -q 'hola paquete' "$tmp/git_run.log"; then
        pass "git_dependencia"
    else
        fail "git_dependencia"
        sed 's/^/      /' "$tmp/git_install.log"
        sed 's/^/      /' "$tmp/git_run.log" 2>/dev/null
    fi

    mkdir -p "$tmp/addgit"
    cp "$dir/fixtures/app/main.ro" "$tmp/addgit/main.ro"

    cat > "$tmp/addgit/rivo.toml" <<'EOF'
[proyecto]
nombre = "app_add_git"
version = "0.1.0"
entrada = "main.ro"
EOF

    if (cd "$tmp/addgit" && "$rivo" add "file://$tmp/gitpkg#v0.1.0") > "$tmp/addgit.log" 2>&1 &&
       grep -q 'git = "file://' "$tmp/addgit/rivo.toml" &&
       grep -q 'tag = "v0.1.0"' "$tmp/addgit/rivo.toml" &&
       (cd "$tmp/addgit" && "$rivo" main.ro --run) > "$tmp/addgit_run.log" 2>&1 &&
       grep -q 'hola paquete' "$tmp/addgit_run.log"; then
        pass "add_git"
    else
        fail "add_git"
        sed 's/^/      /' "$tmp/addgit.log"
        sed 's/^/      /' "$tmp/addgit_run.log" 2>/dev/null
    fi

    mkdir -p "$tmp/registry/paquetes/saludo"
    git init -q "$tmp/registry"
    git -C "$tmp/registry" config user.email tests@rivo
    git -C "$tmp/registry" config user.name tests

    cat > "$tmp/registry/paquetes/saludo/0.1.0.toml" <<EOF
[paquete]
nombre = "saludo"
version = "0.1.0"
git = "file://$tmp/gitpkg"
tag = "v0.1.0"
descripcion = "Saludo de prueba"
EOF

    git -C "$tmp/registry" add .
    git -C "$tmp/registry" commit -qm inicial

    mkdir -p "$tmp/regapp"
    cp "$dir/fixtures/app/main.ro" "$tmp/regapp/main.ro"

    cat > "$tmp/regapp/rivo.toml" <<EOF
[proyecto]
nombre = "app_registro"
version = "0.1.0"
entrada = "main.ro"

[registro]
url = "file://$tmp/registry"

[dependencias]
saludo = "^0.1.0"
EOF

    if (cd "$tmp/regapp" && "$rivo" install) > "$tmp/reg_install.log" 2>&1 &&
       grep -q 'version = "0.1.0"' "$tmp/regapp/rivo.lock" &&
       (cd "$tmp/regapp" && "$rivo" main.ro --run) > "$tmp/reg_run.log" 2>&1 &&
       grep -q 'hola paquete' "$tmp/reg_run.log"; then
        pass "registro_install"
    else
        fail "registro_install"
        sed 's/^/      /' "$tmp/reg_install.log"
        sed 's/^/      /' "$tmp/reg_run.log" 2>/dev/null
    fi

    if (cd "$tmp/regapp" && "$rivo" search saludo) > "$tmp/search.log" 2>&1 &&
       grep -q 'saludo 0.1.0' "$tmp/search.log" &&
       grep -q 'Saludo de prueba' "$tmp/search.log"; then
        pass "registro_search"
    else
        fail "registro_search"
        sed 's/^/      /' "$tmp/search.log"
    fi

    if (cd "$tmp/regapp" && "$rivo" info saludo) > "$tmp/info.log" 2>&1 &&
       grep -q '0.1.0' "$tmp/info.log" &&
       grep -q 'v0.1.0' "$tmp/info.log"; then
        pass "registro_info"
    else
        fail "registro_info"
        sed 's/^/      /' "$tmp/info.log"
    fi

    # ---------------------------------------------------
    # 5. publish -> install desde otra carpeta
    # ---------------------------------------------------

    git init -q --bare "$tmp/registry.git"

    cp -R "$dir/fixtures/libs/saludo" "$tmp/pubpkg"
    cat >> "$tmp/pubpkg/rivo.toml" <<EOF

[registro]
url = "file://$tmp/registry.git"
EOF

    git -C "$tmp/pubpkg" init -q
    git -C "$tmp/pubpkg" config user.email tests@rivo
    git -C "$tmp/pubpkg" config user.name tests
    git -C "$tmp/pubpkg" add .
    git -C "$tmp/pubpkg" commit -qm inicial
    git -C "$tmp/pubpkg" tag v0.1.0
    git -C "$tmp/pubpkg" remote add origin "file://$tmp/pubpkg"

    mkdir -p "$tmp/pubapp"
    cp "$dir/fixtures/app/main.ro" "$tmp/pubapp/main.ro"

    cat > "$tmp/pubapp/rivo.toml" <<EOF
[proyecto]
nombre = "app_publish"
version = "0.1.0"
entrada = "main.ro"

[registro]
url = "file://$tmp/registry.git"

[dependencias]
saludo = "^0.1.0"
EOF

    if (cd "$tmp/pubpkg" && "$rivo" publish v0.1.0) > "$tmp/publish.log" 2>&1 &&
       git -C "$tmp/registry.git" cat-file -e HEAD:paquetes/saludo/0.1.0.toml 2>/dev/null &&
       (cd "$tmp/pubapp" && "$rivo" install) > "$tmp/pub_install.log" 2>&1 &&
       (cd "$tmp/pubapp" && "$rivo" main.ro --run) > "$tmp/pub_run.log" 2>&1 &&
       grep -q 'hola paquete' "$tmp/pub_run.log"; then
        pass "publish_install"
    else
        fail "publish_install"
        sed 's/^/      /' "$tmp/publish.log"
        sed 's/^/      /' "$tmp/pub_install.log" 2>/dev/null
        sed 's/^/      /' "$tmp/pub_run.log" 2>/dev/null
    fi
fi

if [ "$fails" -ne 0 ]; then
    echo "PACKAGE TESTS: $fails FALLOS"
    exit 1
fi

echo "PACKAGE TESTS: PASS"
exit 0
