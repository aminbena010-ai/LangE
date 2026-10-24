# Makefile — E++
#
# Envoltorio sobre CMake.
#
#   make            compila en build/ (binario `rivo`)
#   make install    instala en ~/.local (binario `rivo`,
#                   stdlib y runtime; rutas internas)
#   make test       ejecuta la suite de tests
#   make uninstall  desinstala
#   make clean      borra build/
#
# El destino se cambia con:  make install PREFIX=/otra/ruta

BUILD  ?= build
PREFIX ?= $(HOME)/.local
JOBS   ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all build install uninstall test test-runtime bench clean

all: build

build:
	cmake -S . -B $(BUILD) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	cmake --build $(BUILD) -j$(JOBS)

install: build
	cmake --install $(BUILD) --prefix $(PREFIX)
	@echo ""
	@echo "Instalado en $(PREFIX):"
	@echo "  $(PREFIX)/bin/rivo"
	@echo "  $(PREFIX)/share/rivo/lib   (biblioteca estándar)"
	@echo "  $(PREFIX)/lib/librivo_runtime.a   (runtime)"

uninstall:
	rm -f  $(PREFIX)/bin/rivo
	rm -f  $(PREFIX)/lib/librivo_runtime.a
	rm -rf $(PREFIX)/share/rivo

test: build
	sh tests/run_tests.sh

test-runtime:
	sh tests/runtime/run_tests.sh

bench: build
	sh tests/bench/bench.sh

clean:
	rm -rf $(BUILD)
