cmake_minimum_required(VERSION 3.16)

project(rivo LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------
# Instalación (make install)
#
# Destino por defecto: ~/.local en Unix y LocalAppData en Windows.
#   bin/rivo                    compilador (renombrado)
#   lib/librivo_runtime.a/.lib  runtime para enlazar
#   share/rivo/lib/*.ro        biblioteca estándar
#
# El binario resuelve la stdlib y el runtime relativos a su
# propia ubicación (rutas internas), sin variables de
# entorno ni configuración externa.
# ----------------------------------------------------------

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(WIN32)
        if(DEFINED ENV{LOCALAPPDATA})
            set(RIVO_DEFAULT_PREFIX "$ENV{LOCALAPPDATA}/Programs/Rivo")
        else()
            set(RIVO_DEFAULT_PREFIX "$ENV{USERPROFILE}/AppData/Local/Programs/Rivo")
        endif()
    else()
        set(RIVO_DEFAULT_PREFIX "$ENV{HOME}/.local")
    endif()
    set(CMAKE_INSTALL_PREFIX "${RIVO_DEFAULT_PREFIX}" CACHE PATH
        "Directorio de instalación por defecto" FORCE)
endif()

# ----------------------------------------------------------
# LLVM
# ----------------------------------------------------------
# 1) find_package(LLVM) oficial (LLVMConfig.cmake).
# 2) Si no está en el path de CMake, se localiza llvm-config
#    y se usa su directorio cmake (--cmakedir).
# ----------------------------------------------------------

find_package(LLVM CONFIG QUIET)

if(NOT LLVM_FOUND)
    find_program(LLVM_CONFIG_EXECUTABLE
        NAMES
            llvm-config
            llvm-config-21
            llvm-config-22
            llvm-config-20
            llvm-config-19
            llvm-config-18
            llvm-config-17
    )

    if(LLVM_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${LLVM_CONFIG_EXECUTABLE} --cmakedir
            OUTPUT_VARIABLE LLVM_CMAKE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE LLVM_CONFIG_RESULT
        )

        if(LLVM_CONFIG_RESULT EQUAL 0 AND LLVM_CMAKE_DIR)
            find_package(LLVM CONFIG REQUIRED HINTS ${LLVM_CMAKE_DIR})
        endif()
    endif()
endif()

if(NOT LLVM_FOUND)
    message(FATAL_ERROR
        "No se encontró LLVM.\n"
        "Instale LLVM/development headers o establezca LLVM_DIR "
        "apuntando al directorio que contiene LLVMConfig.cmake."
    )
endif()

message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM cmake dir: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})

if(LLVM_DEFINITIONS)
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    add_definitions(${LLVM_DEFINITIONS_LIST})
endif()

# ----------------------------------------------------------
# Runtime de E++
#
# Se enlaza contra los programas generados por `rivo`, no
# contra el propio compilador. Se compila igualmente como
# parte del build normal.
# ----------------------------------------------------------

add_library(rivo_runtime STATIC
    src/runtime/core.c
    src/runtime/lists.c
    src/runtime/maps.c
    src/runtime/time.c
    src/runtime/text.c
    src/runtime/files.c
    src/runtime/console.c
    src/runtime/async.c
)

if(WIN32)
    set_target_properties(rivo_runtime PROPERTIES
        OUTPUT_NAME "librivo_runtime"
    )
endif()

target_include_directories(rivo_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ----------------------------------------------------------
# Ejecutable
# ----------------------------------------------------------

add_executable(rivo
    src/token.c
    src/lexer.c
    src/parser.c
    src/semantic.c
    src/error.c
    src/suggest.c
    src/ast.c
    src/llvm.c
    src/linker.c
    src/manifest.c
    src/semver.c
    src/lockfile.c
    src/packages.c
    src/build.c
    src/hash.c
    src/formatter.c
    src/main.c
)

target_include_directories(rivo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# `rivo` necesita saber dónde está el runtime para poder
# enlazarlo (linker_set_runtime).
add_dependencies(rivo rivo_runtime)

target_compile_definitions(rivo PRIVATE
    "RIVO_RUNTIME_PATH=\"$<TARGET_FILE:rivo_runtime>\""
    "RIVO_STDLIB_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/lib\""
)

# ----------------------------------------------------------
# Enlace con LLVM
# ----------------------------------------------------------

if(TARGET LLVM)
    set(RIVO_LLVM_LIBS LLVM)
else()
    llvm_map_components_to_libnames(RIVO_LLVM_LIBS
        core
        analysis
        target
        targetmc
        bitwriter
        passes
        support
    )
endif()

# `m`: matemática del runtime (fmod, sqrt, ...). Sin -lm el
# enlazador falla en Linux.
target_link_libraries(rivo PRIVATE ${RIVO_LLVM_LIBS} m)

# ----------------------------------------------------------
# Reglas de instalación
#
# Destinos fijos (no GNUInstallDirs) para que coincidan
# exactamente con las rutas internas que el binario deriva
# de su propia ubicación:
#   <bin>/../share/rivo/lib   stdlib
#   <bin>/../lib/librivo_runtime.a/.lib   runtime
# ----------------------------------------------------------

# El binario se instala como `rivo`. Se usa install(PROGRAMS)
# + RENAME porque install(TARGETS) no renombra el artefacto.
install(PROGRAMS $<TARGET_FILE:rivo>
    DESTINATION bin
    RENAME "rivo${CMAKE_EXECUTABLE_SUFFIX}"
)

install(TARGETS rivo_runtime
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/
    DESTINATION share/rivo/lib
    FILES_MATCHING
    PATTERN "*.ro"
    PATTERN "*.c"
)
