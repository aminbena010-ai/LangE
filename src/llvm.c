#include "llvm.h"
#include "semantic.h"
#include "runtime.h"
#include "builtins.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* =========================================================
 * UTILIDADES
 * ========================================================= */

static char *llvm_strdup(const char *text)
{
    if (text == NULL)
        return NULL;

    size_t length = strlen(text);

    char *copy = malloc(length + 1);

    if (copy == NULL)
        return NULL;

    memcpy(copy, text, length + 1);

    return copy;
}


static void llvm_report_error(
    LLVMCompiler *compiler,
    ErrorKind kind,
    int line,
    int column,
    const char *message
)
{
    if (compiler == NULL)
        return;

    compiler->had_error = 1;

    if (compiler->errors != NULL) {

        error_report_simple(
            compiler->errors,
            kind,
            ERROR_PHASE_CODEGEN,
            line,
            column,
            message
        );
    }
}


static void llvm_report_internal(
    LLVMCompiler *compiler,
    const char *message
)
{
    llvm_report_error(
        compiler,
        ERROR_INTERNAL_INVALID_STATE,
        0,
        0,
        message
    );
}


/* =========================================================
 * INICIALIZACIÓN
 * ========================================================= */

int llvm_initialize(void)
{
    /*
     * Inicialización de targets LLVM.
     *
     * Estas funciones están disponibles en la API C
     * de LLVM en las versiones habituales.
     */

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    return 1;
}


void llvm_shutdown(void)
{
    /*
     * LLVMShutdown() existe en algunas versiones de la
     * API C, pero no es necesario para liberar un módulo
     * concreto.
     *
     * Se deja vacío para mantener compatibilidad.
     */
}


/* =========================================================
 * CONFIGURACIÓN
 * ========================================================= */

LLVMConfig llvm_default_config(void)
{
    LLVMConfig config;

    config.module_name = "programa";
    config.target_triple = NULL;
    config.target = LLVM_TARGET_NATIVE;
    config.optimization_level = 0;
    config.debug_info = 0;
    config.position_independent = 0;
    config.output_type = LLVM_OUTPUT_IR;

    return config;
}


const char *llvm_target_name(LLVMTarget target)
{
    switch (target) {

        case LLVM_TARGET_NATIVE:
            return "native";

        case LLVM_TARGET_X86_64_WINDOWS:
            return "x86_64-windows";

        case LLVM_TARGET_X86_64_LINUX:
            return "x86_64-linux";

        case LLVM_TARGET_ARM64_WINDOWS:
            return "arm64-windows";

        case LLVM_TARGET_ARM64_LINUX:
            return "arm64-linux";

        default:
            return "unknown";
    }
}


static const char *llvm_target_default_triple(
    LLVMTarget target
)
{
    switch (target) {

        case LLVM_TARGET_X86_64_WINDOWS:
            return "x86_64-pc-windows-msvc";

        case LLVM_TARGET_X86_64_LINUX:
            return "x86_64-pc-linux-gnu";

        case LLVM_TARGET_ARM64_WINDOWS:
            return "aarch64-pc-windows-msvc";

        case LLVM_TARGET_ARM64_LINUX:
            return "aarch64-pc-linux-gnu";

        case LLVM_TARGET_NATIVE:
        default:
            return NULL;
    }
}


/* =========================================================
 * CREACIÓN / DESTRUCCIÓN
 * ========================================================= */

LLVMCompiler *llvm_create(
    const LLVMConfig *config,
    ErrorContext *errors
)
{
    LLVMCompiler *compiler =
        calloc(1, sizeof(LLVMCompiler));

    if (compiler == NULL)
        return NULL;

    LLVMConfig default_config =
        llvm_default_config();

    if (config != NULL)
        compiler->config = *config;
    else
        compiler->config = default_config;

    compiler->errors = errors;

    compiler->context =
        LLVMContextCreate();

    if (compiler->context == NULL) {

        free(compiler);
        return NULL;
    }

    const char *module_name =
        compiler->config.module_name;

    if (module_name == NULL)
        module_name = "programa";

    compiler->module =
        LLVMModuleCreateWithNameInContext(
            module_name,
            compiler->context
        );

    if (compiler->module == NULL) {

        LLVMContextDispose(
            compiler->context
        );

        free(compiler);
        return NULL;
    }

    compiler->builder =
        LLVMCreateBuilderInContext(
            compiler->context
        );

    if (compiler->builder == NULL) {

        LLVMDisposeModule(
            compiler->module
        );

        LLVMContextDispose(
            compiler->context
        );

        free(compiler);
        return NULL;
    }

    /*
     * Inicializar target.
     */
    if (!llvm_set_target(
            compiler,
            compiler->config.target
        )) {

        llvm_destroy(compiler);
        return NULL;
    }

    return compiler;
}


void llvm_destroy(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL)
        return;

    if (compiler->builder != NULL)
        LLVMDisposeBuilder(
            compiler->builder
        );

    if (compiler->target_machine != NULL)
        LLVMDisposeTargetMachine(
            compiler->target_machine
        );

    if (compiler->module != NULL)
        LLVMDisposeModule(
            compiler->module
        );

    if (compiler->context != NULL)
        LLVMContextDispose(
            compiler->context
        );

    free(compiler->target_triple);
    free(compiler->structs);

    free(compiler);
}


/* =========================================================
 * TARGET
 * ========================================================= */

int llvm_set_target(
    LLVMCompiler *compiler,
    LLVMTarget target
)
{
    if (compiler == NULL)
        return 0;

    compiler->config.target = target;

    const char *triple =
        llvm_target_default_triple(target);

    if (triple == NULL) {

        char *native_triple =
            LLVMGetDefaultTargetTriple();

        if (native_triple == NULL) {

            llvm_report_internal(
                compiler,
                "no se pudo obtener el target nativo"
            );

            return 0;
        }

        int result =
            llvm_set_target_triple(
                compiler,
                native_triple
            );

        LLVMDisposeMessage(native_triple);

        return result;
    }

    return llvm_set_target_triple(
        compiler,
        triple
    );
}


int llvm_set_target_triple(
    LLVMCompiler *compiler,
    const char *triple
)
{
    if (compiler == NULL ||
        triple == NULL)
        return 0;

    free(compiler->target_triple);

    compiler->target_triple =
        llvm_strdup(triple);

    if (compiler->target_triple == NULL) {

        llvm_report_internal(
            compiler,
            "no se pudo guardar el target triple"
        );

        return 0;
    }

    compiler->config.target_triple =
        compiler->target_triple;

    LLVMSetTarget(
        compiler->module,
        compiler->target_triple
    );

    /*
     * Obtener target.
     */
    char *error_message = NULL;

    LLVMTargetRef target = NULL;

    if (LLVMGetTargetFromTriple(
            compiler->target_triple,
            &target,
            &error_message
        ) != 0) {

        if (compiler->errors != NULL &&
            error_message != NULL) {

            error_report_full(
                compiler->errors,
                ERROR_CODEGEN_TARGET_ERROR,
                ERROR_PHASE_CODEGEN,
                0,
                0,
                0,
                "target LLVM invalido",
                error_message,
                NULL
            );
        }

        if (error_message != NULL)
            LLVMDisposeMessage(error_message);

        compiler->had_error = 1;

        return 0;
    }

    compiler->target = target;

    /*
     * Crear target machine.
     */
    if (compiler->target_machine != NULL) {

        LLVMDisposeTargetMachine(
            compiler->target_machine
        );

        compiler->target_machine = NULL;
    }

    LLVMCodeGenOptLevel opt_level =
        LLVMCodeGenLevelNone;

    switch (compiler->config.optimization_level) {

        case 0:
            opt_level = LLVMCodeGenLevelNone;
            break;

        case 1:
            opt_level = LLVMCodeGenLevelLess;
            break;

        case 2:
            opt_level = LLVMCodeGenLevelDefault;
            break;

        case 3:
        default:
            opt_level = LLVMCodeGenLevelAggressive;
            break;
    }

    compiler->target_machine =
        LLVMCreateTargetMachine(
            compiler->target,
            compiler->target_triple,
            "",
            "",
            opt_level,
            /*
             * PIC: los enlazadores modernos producen PIE por
             * defecto y rechazan objetos con relocaciones
             * absolutas (R_X86_64_32).
             */
            LLVMRelocPIC,
            LLVMCodeModelDefault
        );

    if (compiler->target_machine == NULL) {

        llvm_report_internal(
            compiler,
            "no se pudo crear el target machine"
        );

        return 0;
    }

    return 1;
}


const char *llvm_get_target_triple(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL)
        return NULL;

    return compiler->target_triple;
}


/* =========================================================
 * TIPOS LLVM
 * ========================================================= */

LLVMTypeRef llvm_int_type(
    LLVMCompiler *compiler
)
{
    return LLVMInt64TypeInContext(
        compiler->context
    );
}


LLVMTypeRef llvm_float_type(
    LLVMCompiler *compiler
)
{
    return LLVMDoubleTypeInContext(
        compiler->context
    );
}


LLVMTypeRef llvm_text_type(
    LLVMCompiler *compiler
)
{
    /*
     * string = i8*
     */
    return LLVMPointerType(
        LLVMInt8TypeInContext(
            compiler->context
        ),
        0
    );
}


LLVMTypeRef llvm_boolean_type(
    LLVMCompiler *compiler
)
{
    return LLVMInt1TypeInContext(
        compiler->context
    );
}


LLVMTypeRef llvm_char_type(
    LLVMCompiler *compiler
)
{
    return LLVMInt8TypeInContext(
        compiler->context
    );
}


LLVMTypeRef llvm_list_type(
    LLVMCompiler *compiler,
    const Type *element_type
)
{
    /*
     * list<T> se representa como una referencia opaca a un
     * objeto del runtime (struct RivoList de runtime.c):
     *
     *     ptr          -> puntero a la lista
     *
     * El backend no conoce el interior: lee, escribe,
     * amplía y comprueba límites llamando al runtime
     * (claude.md §11 y §26). Así la representación es la
     * misma para todo T y no hay GEP sin comprobar.
     */

    (void) element_type;

    return LLVMPointerType(
        LLVMInt8TypeInContext(compiler->context),
        0
    );
}


LLVMTypeRef llvm_map_type(
    LLVMCompiler *compiler,
    const Type *key_type,
    const Type *value_type
)
{
    /*
     * map<K, V> se representa como una referencia opaca a
     * un objeto del runtime (struct RivoMap de runtime.c):
     *
     *     ptr          -> puntero al map
     *
     * Lectura, escritura, inserción de claves y recorrido
     * van al runtime, que es donde viven las entradas
     * (claude.md §12).
     */

    (void)key_type;
    (void)value_type;

    return LLVMPointerType(
        LLVMInt8TypeInContext(compiler->context),
        0
    );
}


static LLVMStructInfo *llvm_find_struct(
    LLVMCompiler *compiler,
    const char *name
)
{
    if (compiler == NULL || name == NULL)
        return NULL;

    for (int i = 0; i < compiler->struct_count; i++) {
        ASTNode *declaration =
            compiler->structs[i].declaration;

        if (declaration != NULL &&
            declaration->data.struct_decl.name != NULL &&
            strcmp(
                declaration->data.struct_decl.name,
                name
            ) == 0)
            return &compiler->structs[i];
    }

    return NULL;
}


static int llvm_add_struct_type(
    LLVMCompiler *compiler,
    ASTNode *declaration
)
{
    if (compiler == NULL ||
        declaration == NULL ||
        declaration->kind != AST_STRUCT_DECL)
        return 0;

    const char *name =
        declaration->data.struct_decl.name;

    if (name == NULL)
        return 0;

    LLVMStructInfo *existing =
        llvm_find_struct(compiler, name);

    if (existing != NULL) {
        if (existing->declaration == declaration)
            return 1;

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            declaration->line,
            declaration->column,
            "hay structs con el mismo nombre en modulos distintos"
        );
        return 0;
    }

    if (compiler->struct_count >= compiler->struct_capacity) {
        int capacity =
            compiler->struct_capacity == 0
                ? 8
                : compiler->struct_capacity * 2;

        LLVMStructInfo *grown =
            realloc(
                compiler->structs,
                (size_t) capacity * sizeof(LLVMStructInfo)
            );

        if (grown == NULL) {
            llvm_report_internal(
                compiler,
                "no se pudo reservar memoria para los structs"
            );
            return 0;
        }

        compiler->structs = grown;
        compiler->struct_capacity = capacity;
    }

    char llvm_name[512];
    int length =
        snprintf(
            llvm_name,
            sizeof(llvm_name),
            "rivo.struct.%s",
            name
        );

    if (length < 0 ||
        (size_t) length >= sizeof(llvm_name)) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            declaration->line,
            declaration->column,
            "nombre de struct demasiado largo"
        );
        return 0;
    }

    LLVMTypeRef type =
        LLVMStructCreateNamed(
            compiler->context,
            llvm_name
        );

    if (type == NULL)
        return 0;

    compiler->structs[compiler->struct_count++] =
        (LLVMStructInfo) {
            .declaration = declaration,
            .type = type
        };

    return 1;
}


static int llvm_collect_struct_types(
    LLVMCompiler *compiler,
    ASTNodeList *declarations
)
{
    if (compiler == NULL ||
        declarations == NULL)
        return 0;

    int valid = 1;

    for (int i = 0; i < declarations->count; i++) {
        ASTNode *node = declarations->items[i];

        if (node == NULL)
            continue;

        if (node->kind == AST_STRUCT_DECL) {
            if (!llvm_add_struct_type(compiler, node))
                valid = 0;
            continue;
        }

        ASTNodeList *nested = NULL;

        if (node->kind == AST_IMPORT)
            nested = &node->data.import_decl.declarations;
        else if (node->kind == AST_FROM_IMPORT)
            nested = &node->data.from_import.declarations;

        if (nested != NULL &&
            !llvm_collect_struct_types(compiler, nested))
            valid = 0;
    }

    return valid;
}


static int llvm_complete_struct_types(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL)
        return 0;

    for (int i = 0; i < compiler->struct_count; i++) {
        LLVMStructInfo *info = &compiler->structs[i];

        if (!LLVMIsOpaqueStruct(info->type))
            continue;

        ASTNodeList *fields =
            &info->declaration->data.struct_decl.fields;

        LLVMTypeRef *field_types =
            fields->count > 0
                ? calloc((size_t) fields->count,
                         sizeof(LLVMTypeRef))
                : NULL;

        if (fields->count > 0 &&
            field_types == NULL) {
            llvm_report_internal(
                compiler,
                "no se pudieron reservar los tipos de campo"
            );
            return 0;
        }

        for (int field = 0; field < fields->count; field++) {
            ASTNode *field_node = fields->items[field];
            field_types[field] =
                llvm_type(
                    compiler,
                    field_node->data.var_decl.type
                );

            if (field_types[field] == NULL) {
                free(field_types);
                return 0;
            }
        }

        LLVMStructSetBody(
            info->type,
            field_types,
            (unsigned) fields->count,
            0
        );
        free(field_types);
    }

    return 1;
}


static int llvm_prepare_struct_types(
    LLVMCompiler *compiler,
    ASTNode *program
)
{
    if (compiler == NULL ||
        program == NULL ||
        program->kind != AST_PROGRAM)
        return 0;

    if (!llvm_collect_struct_types(
            compiler,
            &program->data.program.declarations
        ))
        return 0;

    return llvm_complete_struct_types(compiler);
}


/* Declaración adelantada (definida más abajo). */
static LLVMTypeRef llvm_opaque_pointer(
    LLVMCompiler *compiler
);


LLVMTypeRef llvm_type(
    LLVMCompiler *compiler,
    const Type *type
)
{
    if (compiler == NULL ||
        type == NULL)
        return LLVMVoidTypeInContext(
            compiler->context
        );

    switch (type->kind) {

        case TYPE_INT:
            return llvm_int_type(compiler);

        case TYPE_FLOAT:
            return llvm_float_type(compiler);

        case TYPE_STRING:
            return llvm_text_type(compiler);

        case TYPE_BOOL:
            return llvm_boolean_type(compiler);

        case TYPE_CHAR:
            return llvm_char_type(compiler);

        case TYPE_I8:
            return LLVMInt8TypeInContext(
                compiler->context
            );

        case TYPE_I16:
            return LLVMInt16TypeInContext(
                compiler->context
            );

        case TYPE_I32:
            return LLVMInt32TypeInContext(
                compiler->context
            );

        case TYPE_U8:
            return LLVMInt8TypeInContext(
                compiler->context
            );

        case TYPE_U16:
            return LLVMInt16TypeInContext(
                compiler->context
            );

        case TYPE_U32:
            return LLVMInt32TypeInContext(
                compiler->context
            );

        case TYPE_U64:
            return LLVMInt64TypeInContext(
                compiler->context
            );

        /*
         * size_t: se representa como i64 (target de 64
         * bits por defecto). Con targets de 32 bits
         * cambiaría a LLVMIntPtrType.
         */
        case TYPE_SIZE:
            return LLVMInt64TypeInContext(
                compiler->context
            );

        /*
         * opaque (void*) y callback (puntero a función):
         * con punteros opacos de LLVM 15+ ambos son `ptr`.
         */
        case TYPE_OPAQUE:
        case TYPE_CALLBACK:
        case TYPE_BUFFER:
            return llvm_opaque_pointer(compiler);

        case TYPE_LIST:
            return llvm_list_type(
                compiler,
                type->element_type
            );

        case TYPE_MAP:
            return llvm_map_type(
                compiler,
                type->key_type,
                type->value_type
            );

        case TYPE_NULL:
            return LLVMPointerType(
                LLVMInt8TypeInContext(
                    compiler->context
                ),
                0
            );

        case TYPE_FUNCTION:
            return LLVMPointerType(
                LLVMInt8TypeInContext(compiler->context),
                0
            );

        case TYPE_TASK:
            return LLVMPointerType(
                LLVMInt8TypeInContext(compiler->context),
                0
            );

        case TYPE_STRUCT:
        {
            LLVMStructInfo *info =
                llvm_find_struct(
                    compiler,
                    type->struct_name
                );

            if (info == NULL) {
                llvm_report_error(
                    compiler,
                    ERROR_CODEGEN_INVALID_TYPE,
                    0,
                    0,
                    "tipo de struct sin declaracion LLVM"
                );
                return LLVMVoidTypeInContext(
                    compiler->context
                );
            }

            return LLVMPointerType(info->type, 0);
        }

        default:
            return LLVMVoidTypeInContext(
                compiler->context
            );
    }
}


/* =========================================================
 * VALORES LITERALES
 * ========================================================= */

LLVMValueRef llvm_generate_int(
    LLVMCompiler *compiler,
    long long value
)
{
    return LLVMConstInt(
        llvm_int_type(compiler),
        (unsigned long long)value,
        1
    );
}


LLVMValueRef llvm_generate_float(
    LLVMCompiler *compiler,
    double value
)
{
    return LLVMConstReal(
        llvm_float_type(compiler),
        value
    );
}


LLVMValueRef llvm_generate_string(
    LLVMCompiler *compiler,
    const char *value
)
{
    if (value == NULL)
        value = "";

    return LLVMBuildGlobalStringPtr(
        compiler->builder,
        value,
        ".str"
    );
}


LLVMValueRef llvm_generate_char(
    LLVMCompiler *compiler,
    char value
)
{
    return LLVMConstInt(
        llvm_char_type(compiler),
        (unsigned char)value,
        0
    );
}


LLVMValueRef llvm_generate_boolean(
    LLVMCompiler *compiler,
    int value
)
{
    return LLVMConstInt(
        llvm_boolean_type(compiler),
        value ? 1 : 0,
        0
    );
}


LLVMValueRef llvm_generate_null(
    LLVMCompiler *compiler
)
{
    LLVMTypeRef type =
        LLVMPointerType(
            LLVMInt8TypeInContext(
                compiler->context
            ),
            0
        );

    return LLVMConstNull(type);
}


/* =========================================================
 * COLECCIONES: ETIQUETAS, CONVERSIONES Y RUNTIME
 *
 * list<T> y map<K, V> son referencias opacas a objetos de
 * runtime.c. Todo valor que entra o sale de una colección
 * se transporta como int64_t con el patrón de bits real
 * (runtime.h define las etiquetas compartidas).
 *
 * - llvm_value_to_i64()    valor E++  -> int64_t del runtime
 * - llvm_value_from_i64()  int64_t    -> valor E++
 * - llvm_call_runtime()    declara y llama una función runtime
 * ======================================================= */

static LLVMValueRef llvm_add_runtime_function(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef return_type,
    LLVMTypeRef *parameter_types,
    int parameter_count
);


static LLVMTypeRef llvm_opaque_pointer(
    LLVMCompiler *compiler
)
{
    return LLVMPointerType(
        LLVMInt8TypeInContext(compiler->context),
        0
    );
}


/*
 * Etiqueta de runtime para un tipo E++.
 * 0 si el tipo no puede viajar por una colección.
 */
static int llvm_type_tag(const Type *type)
{
    if (type == NULL)
        return 0;

    switch (type->kind) {

        case TYPE_INT:
            return RIVO_TAG_INT;

        case TYPE_FLOAT:
            return RIVO_TAG_FLOAT;

        case TYPE_STRING:
            return RIVO_TAG_STRING;

        case TYPE_BOOL:
            return RIVO_TAG_BOOL;

        case TYPE_CHAR:
            return RIVO_TAG_CHAR;

        default:
            return 0;
    }
}


static LLVMValueRef llvm_value_to_i64(
    LLVMCompiler *compiler,
    LLVMValueRef value,
    int line,
    int column
)
{
    if (value == NULL)
        return NULL;

    LLVMTypeRef type = LLVMTypeOf(value);

    switch (LLVMGetTypeKind(type)) {

        case LLVMIntegerTypeKind:

        {
            unsigned width =
                LLVMGetIntTypeWidth(type);

            if (width == 64)
                return value;

            if (width < 64)
                return LLVMBuildZExt(
                    compiler->builder,
                    value,
                    llvm_int_type(compiler),
                    "rivo.bits"
                );

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo entero no soportado por el runtime"
            );

            return NULL;
        }

        case LLVMDoubleTypeKind:

            return LLVMBuildBitCast(
                compiler->builder,
                value,
                llvm_int_type(compiler),
                "rivo.bits"
            );

        case LLVMPointerTypeKind:

            return LLVMBuildPtrToInt(
                compiler->builder,
                value,
                llvm_int_type(compiler),
                "rivo.bits"
            );

        default:

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo no soportado por el runtime"
            );

            return NULL;
    }
}


static LLVMValueRef llvm_value_from_i64(
    LLVMCompiler *compiler,
    LLVMValueRef value,
    const Type *type,
    int line,
    int column
)
{
    if (value == NULL ||
        type == NULL)
        return NULL;

    switch (type->kind) {

        case TYPE_INT:
            return value;

        case TYPE_FLOAT:

            return LLVMBuildBitCast(
                compiler->builder,
                value,
                llvm_float_type(compiler),
                "rivo.float"
            );

        case TYPE_BOOL:

            return LLVMBuildTrunc(
                compiler->builder,
                value,
                llvm_boolean_type(compiler),
                "rivo.bool"
            );

        case TYPE_CHAR:

            return LLVMBuildTrunc(
                compiler->builder,
                value,
                llvm_char_type(compiler),
                "rivo.char"
            );

        case TYPE_STRING:
        case TYPE_LIST:
        case TYPE_MAP:
        case TYPE_TASK:
        case TYPE_NULL:

            return LLVMBuildIntToPtr(
                compiler->builder,
                value,
                llvm_opaque_pointer(compiler),
                "rivo.ptr"
            );

        case TYPE_STRUCT:

            return LLVMBuildIntToPtr(
                compiler->builder,
                value,
                llvm_type(compiler, type),
                "rivo.struct"
            );

        default:

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo no soportado por el runtime"
            );

            return NULL;
    }
}


/*
 * Declara (si hace falta) y llama a una función del runtime.
 *
 * Una función void no puede llevar nombre de resultado:
 * LLVM recibe "" porque Twine no tolera punteros nulos.
 */
static LLVMValueRef llvm_call_runtime(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef return_type,
    LLVMTypeRef *parameter_types,
    int parameter_count,
    LLVMValueRef *arguments
)
{
    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            name,
            return_type,
            parameter_types,
            parameter_count
        );

    if (function == NULL)
        return NULL;

    LLVMTypeRef function_type =
        LLVMGlobalGetValueType(function);

    const char *result_name = "rivo";

    if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind)
        result_name = "";

    return LLVMBuildCall2(
        compiler->builder,
        function_type,
        function,
        arguments,
        parameter_count,
        result_name
    );
}


/* =========================================================
 * FUNCIONES LLVM
 * ========================================================= */

LLVMValueRef llvm_find_function(
    LLVMCompiler *compiler,
    const char *name
)
{
    if (compiler == NULL ||
        name == NULL)
        return NULL;

    return LLVMGetNamedFunction(
        compiler->module,
        name
    );
}


LLVMValueRef llvm_create_function(
    LLVMCompiler *compiler,
    const char *name,
    const Type *return_type,
    ASTParameterList *parameters,
    int is_variadic,
    int is_async
)
{
    if (compiler == NULL ||
        name == NULL)
        return NULL;

    LLVMTypeRef llvm_return_type;

    /*
     * Una función async devuelve task<T>: en LLVM es un
     * puntero opaco al objeto del runtime.
     */
    if (is_async) {

        llvm_return_type =
            llvm_opaque_pointer(compiler);

    } else if (return_type == NULL) {

        /*
         * `fn main():` se genera como i32: el proceso debe
         * terminar con código de salida 0 cuando el
         * programa no lanza un error (decisión de diseño
         * 2026-10-06). Las demás funciones void siguen
         * siendo void.
         */
        if (strcmp(name, "main") == 0) {

            llvm_return_type =
                LLVMInt32TypeInContext(
                    compiler->context
                );

        } else {

            llvm_return_type =
                LLVMVoidTypeInContext(
                    compiler->context
                );
        }

    } else {

        llvm_return_type =
            llvm_type(
                compiler,
                return_type
            );
    }

    int parameter_count =
        parameters != NULL
            ? parameters->count
            : 0;

    /*
     * buffer<T> se expande en dos parámetros C: (T*, size).
     * Contamos los parámetros reales tras la expansión.
     */
    int llvm_parameter_count = 0;

    for (int i = 0; i < parameter_count; i++) {

        const Type *type =
            parameters->items[i]->type;

        if (type != NULL &&
            type->kind == TYPE_BUFFER)
            llvm_parameter_count += 2;
        else
            llvm_parameter_count++;
    }

    LLVMTypeRef *parameter_types = NULL;

    if (llvm_parameter_count > 0) {

        parameter_types =
            calloc(
                llvm_parameter_count,
                sizeof(LLVMTypeRef)
            );

        if (parameter_types == NULL)
            return NULL;

        int index = 0;

        for (int i = 0;
             i < parameter_count;
             i++) {

            const Type *type =
                parameters->items[i]->type;

            if (type != NULL &&
                type->kind == TYPE_BUFFER) {

                /*
                 * buffer<T> → (T*, int64).
                 * El puntero es al backing store de la
                 * lista (int64* en el runtime).
                 */
                parameter_types[index++] =
                    llvm_opaque_pointer(compiler);

                parameter_types[index++] =
                    llvm_int_type(compiler);

            } else {

                parameter_types[index++] =
                    llvm_type(
                        compiler,
                        type
                    );
            }
        }
    }

    LLVMTypeRef function_type =
        LLVMFunctionType(
            llvm_return_type,
            parameter_types,
            llvm_parameter_count,
            is_variadic ? 1 : 0
        );

    LLVMValueRef function =
        LLVMAddFunction(
            compiler->module,
            name,
            function_type
        );

    free(parameter_types);

    return function;
}


LLVMBasicBlockRef llvm_create_block(
    LLVMCompiler *compiler,
    LLVMValueRef function,
    const char *name
)
{
    if (compiler == NULL ||
        function == NULL)
        return NULL;

    return LLVMAppendBasicBlockInContext(
        compiler->context,
        function,
        name != NULL ? name : "block"
    );
}


/* =========================================================
 * VARIABLES
 * ========================================================= */

static int llvm_push_scope(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL)
        return 0;

    if (compiler->scope_count >= LLVM_MAX_SCOPES) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            0,
            0,
            "demasiados ámbitos anidados"
        );

        return 0;
    }

    compiler->scopes[
        compiler->scope_count
    ].count = 0;

    compiler->scope_count++;

    return 1;
}


static void llvm_pop_scope(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL ||
        compiler->scope_count <= 0)
        return;

    compiler->scope_count--;

    compiler->scopes[
        compiler->scope_count
    ].count = 0;
}


static int llvm_scope_declare(
    LLVMCompiler *compiler,
    const char *name,
    LLVMValueRef variable
)
{
    if (compiler == NULL ||
        name == NULL ||
        compiler->scope_count <= 0)
        return 1;

    LLVMScope *scope =
        &compiler->scopes[
            compiler->scope_count - 1
        ];

    if (scope->count >= LLVM_MAX_SCOPE_VARIABLES) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            0,
            0,
            "demasiadas variables en un ámbito"
        );

        return 0;
    }

    scope->variables[scope->count].name = name;
    scope->variables[scope->count].variable = variable;

    scope->count++;

    return 1;
}


LLVMValueRef llvm_create_variable(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef type
)
{
    if (compiler == NULL ||
        type == NULL)
        return NULL;

    LLVMValueRef variable =
        LLVMBuildAlloca(
            compiler->builder,
            type,
            name != NULL ? name : "var"
        );

    if (variable == NULL)
        return NULL;

    llvm_scope_declare(
        compiler,
        name,
        variable
    );

    return variable;
}


LLVMValueRef llvm_find_variable(
    LLVMCompiler *compiler,
    const char *name
)
{
    /*
     * Esta implementación busca primero en la pila de
     * ámbitos y después mediante allocas dentro de la
     * función actual.
     *
     * La búsqueda en ámbitos resuelve el sombreado: el
     * ámbito interior se consulta antes que el exterior.
     */

    if (compiler == NULL ||
        name == NULL)
        return NULL;

    for (int scope_index = compiler->scope_count - 1;
         scope_index >= 0;
         scope_index--) {

        LLVMScope *scope =
            &compiler->scopes[scope_index];

        for (int i = scope->count - 1;
             i >= 0;
             i--) {

            if (scope->variables[i].name != NULL &&
                strcmp(
                    scope->variables[i].name,
                    name
                ) == 0) {

                return scope->variables[i].variable;
            }
        }
    }

    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    if (function == NULL)
        return NULL;

    for (
        LLVMBasicBlockRef block =
            LLVMGetFirstBasicBlock(function);
        block != NULL;
        block =
            LLVMGetNextBasicBlock(block)
    ) {

        for (
            LLVMValueRef instruction =
                LLVMGetFirstInstruction(block);
            instruction != NULL;
            instruction =
                LLVMGetNextInstruction(instruction)
        ) {

            if (LLVMGetInstructionOpcode(
                    instruction
                ) != LLVMAlloca)
                continue;

            const char *instruction_name =
                LLVMGetValueName(instruction);

            if (instruction_name != NULL &&
                strcmp(
                    instruction_name,
                    name
                ) == 0) {

                return instruction;
            }
        }
    }

    return NULL;
}


LLVMValueRef llvm_load_variable(
    LLVMCompiler *compiler,
    LLVMValueRef variable
)
{
    if (compiler == NULL ||
        variable == NULL)
        return NULL;

    /*
     * Con punteros opacos (LLVM 15+) LLVMTypeOf(variable)
     * es `ptr` y no tiene elemento. El tipo correcto es el
     * que aloja el alloca: LLVMGetAllocatedType().
     */
    LLVMTypeRef value_type =
        LLVMGetAllocatedType(variable);

    return LLVMBuildLoad2(
        compiler->builder,
        value_type,
        variable,
        "load"
    );
}


int llvm_store_variable(
    LLVMCompiler *compiler,
    LLVMValueRef variable,
    LLVMValueRef value
)
{
    if (compiler == NULL ||
        variable == NULL ||
        value == NULL)
        return 0;

    LLVMBuildStore(
        compiler->builder,
        value,
        variable
    );

    return 1;
}


/* =========================================================
 * OPERACIONES
 * ======================================================= */

/* Declarada aquí; se define más abajo (secciones runtime). */
static LLVMValueRef llvm_add_runtime_function(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef return_type,
    LLVMTypeRef *parameter_types,
    int parameter_count
);


/*
 * Llamada a una función runtime de aritmética entera de dos
 * operandos (rivo_add_int, rivo_sub_int, ...). Devuelve NULL
 * y reporta error interno si la declaración falla.
 *
 * Arreglo 2026-10-16: los operadores enteros +, -, *, / y %
 * llaman al runtime en lugar de emitir add/mul/sdiv/srem
 * crudos de LLVM, que desbordaban en silencio.
 */
static LLVMValueRef llvm_build_checked_int_op(
    LLVMCompiler *compiler,
    const char *runtime_name,
    LLVMValueRef left,
    LLVMValueRef right,
    const char *result_name
)
{
    LLVMTypeRef parameter_type =
        LLVMInt64TypeInContext(compiler->context);

    LLVMTypeRef parameter_types[2] = {
        parameter_type,
        parameter_type
    };

    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            runtime_name,
            parameter_type,
            parameter_types,
            2
        );

    if (function == NULL) {
        char message[128];

        snprintf(
            message,
            sizeof(message),
            "no se pudo declarar %s",
            runtime_name
        );

        llvm_report_internal(compiler, message);
        return NULL;
    }

    LLVMValueRef arguments[2] = {left, right};

    return LLVMBuildCall2(
        compiler->builder,
        LLVMGlobalGetValueType(function),
        function,
        arguments,
        2,
        result_name
    );
}


/*
 * Llamada a una función runtime de aritmética float de dos
 * operandos (rivo_float_add, ...). Mismo patrón que
 * llvm_build_checked_int_op: los operadores float llaman al
 * runtime (Aritmética IEEE 754, sin rejilla decimal).
 * Devuelve NULL y reporta error interno si la declaración
 * falla.
 */
static LLVMValueRef llvm_build_float_op(
    LLVMCompiler *compiler,
    const char *runtime_name,
    LLVMValueRef left,
    LLVMValueRef right,
    const char *result_name
)
{
    LLVMTypeRef parameter_type =
        llvm_float_type(compiler);

    LLVMTypeRef parameter_types[2] = {
        parameter_type,
        parameter_type
    };

    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            runtime_name,
            parameter_type,
            parameter_types,
            2
        );

    if (function == NULL) {
        char message[128];

        snprintf(
            message,
            sizeof(message),
            "no se pudo declarar %s",
            runtime_name
        );

        llvm_report_internal(compiler, message);
        return NULL;
    }

    LLVMValueRef arguments[2] = {left, right};

    return LLVMBuildCall2(
        compiler->builder,
        LLVMGlobalGetValueType(function),
        function,
        arguments,
        2,
        result_name
    );
}


/*
 * Mezcla de int y float en una operación.
 *
 * La semántica devuelve float cuando uno de los operandos
 * es float, pero aquí se trabajaba con los tipos literales
 * y LLVM rechazaba el módulo ("Both operands to a binary
 * operator are not of the same type") en cuanto se mezclaban
 * 5.0 y 2. El operando entero se convierte a double.
 */
static void llvm_balance_numeric(
    LLVMCompiler *compiler,
    LLVMValueRef *left,
    LLVMValueRef *right
)
{
    if (compiler == NULL ||
        left == NULL ||
        right == NULL)
        return;

    LLVMTypeKind left_kind =
        LLVMGetTypeKind(
            LLVMTypeOf(*left)
        );

    LLVMTypeKind right_kind =
        LLVMGetTypeKind(
            LLVMTypeOf(*right)
        );

    int left_float =
        left_kind == LLVMDoubleTypeKind ||
        left_kind == LLVMFloatTypeKind;

    int right_float =
        right_kind == LLVMDoubleTypeKind ||
        right_kind == LLVMFloatTypeKind;

    int left_integer =
        left_kind == LLVMIntegerTypeKind;

    int right_integer =
        right_kind == LLVMIntegerTypeKind;

    if (left_float && right_integer) {

        *right =
            LLVMBuildSIToFP(
                compiler->builder,
                *right,
                LLVMTypeOf(*left),
                "a.float"
            );

        return;
    }

    if (right_float && left_integer) {

        *left =
            LLVMBuildSIToFP(
                compiler->builder,
                *left,
                LLVMTypeOf(*right),
                "a.float"
            );
    }
}


LLVMValueRef llvm_build_binary(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef left,
    LLVMValueRef right,
    const Type *type
)
{
    if (compiler == NULL ||
        left == NULL ||
        right == NULL)
        return NULL;

    llvm_balance_numeric(
        compiler,
        &left,
        &right
    );

    LLVMTypeKind kind =
        LLVMGetTypeKind(
            LLVMTypeOf(left)
        );

    int is_float =
        kind == LLVMDoubleTypeKind ||
        kind == LLVMFloatTypeKind;

    switch (operator) {

        case AST_OP_ADD:

            if (is_float)
                return llvm_build_float_op(
                    compiler,
                    "rivo_float_add",
                    left,
                    right,
                    "add"
                );

            /*
             * list + list: concatenación en el runtime.
             *
             * En IR las listas y los strings son punteros
             * opacos iguales, así que hace falta el tipo
             * semántico que llega en `type` para no llamar
             * a rivo_concat_string con structs (antes
             * devolvía una lista vacía; arreglo
             * 2026-10-07).
             */
            if (type != NULL &&
                type->kind == TYPE_LIST) {

                LLVMTypeRef list_parameter_types[2] = {
                    LLVMTypeOf(left),
                    LLVMTypeOf(right)
                };

                LLVMTypeRef list_function_type =
                    LLVMFunctionType(
                        LLVMTypeOf(left),
                        list_parameter_types,
                        2,
                        0
                    );

                LLVMValueRef list_function =
                    llvm_add_runtime_function(
                        compiler,
                        "rivo_concat_list",
                        LLVMTypeOf(left),
                        list_parameter_types,
                        2
                    );

                LLVMValueRef list_arguments[2] = {
                    left,
                    right
                };

                return LLVMBuildCall2(
                    compiler->builder,
                    list_function_type,
                    list_function,
                    list_arguments,
                    2,
                    "concat.list"
                );
            }

            /*
             * string + string: concatenación en el runtime.
             *
             * En la semántica actual los operandos de ADD
             * con tipo puntero solo pueden ser string (las
             * listas y los maps son structs, no punteros).
             * Mismo criterio que print()/len().
             *
             * Propiedad del resultado: arena del runtime,
             * liberada al salir del programa (decisión de
             * diseño 2026-10-04, ver error.log).
             */
            if (kind == LLVMPointerTypeKind &&
                LLVMGetTypeKind(
                    LLVMTypeOf(right)
                ) == LLVMPointerTypeKind) {

                LLVMTypeRef parameter_types[2] = {
                    LLVMTypeOf(left),
                    LLVMTypeOf(right)
                };

                LLVMTypeRef function_type =
                    LLVMFunctionType(
                        LLVMTypeOf(left),
                        parameter_types,
                        2,
                        0
                    );

                LLVMValueRef function =
                    llvm_add_runtime_function(
                        compiler,
                        "rivo_concat_string",
                        LLVMTypeOf(left),
                        parameter_types,
                        2
                    );

                LLVMValueRef arguments[2] = {
                    left,
                    right
                };

                return LLVMBuildCall2(
                    compiler->builder,
                    function_type,
                    function,
                    arguments,
                    2,
                    "rivo.concat"
                );
            }

            return llvm_build_checked_int_op(
                compiler,
                "rivo_add_int",
                left,
                right,
                "add"
            );


        case AST_OP_SUB:

            if (is_float)
                return llvm_build_float_op(
                    compiler,
                    "rivo_float_sub",
                    left,
                    right,
                    "sub"
                );

            return llvm_build_checked_int_op(
                compiler,
                "rivo_sub_int",
                left,
                right,
                "sub"
            );


        case AST_OP_MUL:

            if (is_float)
                return llvm_build_float_op(
                    compiler,
                    "rivo_float_mul",
                    left,
                    right,
                    "mul"
                );

            return llvm_build_checked_int_op(
                compiler,
                "rivo_mul_int",
                left,
                right,
                "mul"
            );


        case AST_OP_DIV:

            if (is_float)
                return llvm_build_float_op(
                    compiler,
                    "rivo_float_div",
                    left,
                    right,
                    "div"
                );

            return llvm_build_checked_int_op(
                compiler,
                "rivo_div_int",
                left,
                right,
                "div"
            );

        case AST_OP_FLOOR_DIV:
        {
            LLVMTypeRef parameter_type =
                LLVMInt64TypeInContext(compiler->context);
            LLVMValueRef function =
                llvm_add_runtime_function(
                    compiler,
                    "rivo_floor_div_int",
                    parameter_type,
                    (LLVMTypeRef[]){parameter_type, parameter_type},
                    2
                );
            if (function == NULL) {
                llvm_report_internal(
                    compiler,
                    "no se pudo declarar rivo_floor_div_int"
                );
                return NULL;
            }
            LLVMValueRef arguments[] = {left, right};

            return LLVMBuildCall2(
                compiler->builder,
                LLVMGlobalGetValueType(function),
                function,
                arguments,
                2,
                "floor.div"
            );
        }


        case AST_OP_MOD:

            if (is_float)
                return llvm_build_float_op(
                    compiler,
                    "rivo_float_rem",
                    left,
                    right,
                    "mod"
                );

            return llvm_build_checked_int_op(
                compiler,
                "rivo_mod_int",
                left,
                right,
                "mod"
            );


        case AST_OP_AND:
            return LLVMBuildAnd(
                compiler->builder,
                left,
                right,
                "and"
            );


        case AST_OP_OR:
            return LLVMBuildOr(
                compiler->builder,
                left,
                right,
                "or"
            );


        default:
            return NULL;
    }
}


/*
 * Comparación de cadenas por contenido.
 *
 * Dos textos con el mismo contenido no tienen por qué
 * compartir dirección de memoria (concatenaciones,
 * subcadenas, valores leídos de un mapa...), así que `==`
 * y `!=` llaman a strcmp en lugar de comparar punteros.
 */
static LLVMValueRef llvm_build_string_compare(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef left,
    LLVMValueRef right
)
{
    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef i32_type =
        LLVMInt32TypeInContext(
            compiler->context
        );

    LLVMTypeRef parameter_types[2] = {
        pointer_type,
        pointer_type
    };

    LLVMTypeRef function_type =
        LLVMFunctionType(
            i32_type,
            parameter_types,
            2,
            0
        );

    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            "strcmp",
            i32_type,
            parameter_types,
            2
        );

    LLVMValueRef arguments[2] = {
        left,
        right
    };

    LLVMValueRef cmp =
        LLVMBuildCall2(
            compiler->builder,
            function_type,
            function,
            arguments,
            2,
            "cad.comparada"
        );

    LLVMValueRef zero =
        LLVMConstInt(
            i32_type,
            0,
            0
        );

    LLVMIntPredicate predicate;

    switch (operator) {

        case AST_OP_EQUAL:
            predicate = LLVMIntEQ;
            break;

        case AST_OP_NOT_EQUAL:
            predicate = LLVMIntNE;
            break;

        case AST_OP_GREATER:
            predicate = LLVMIntSGT;
            break;

        case AST_OP_LESS:
            predicate = LLVMIntSLT;
            break;

        case AST_OP_GREATER_EQUAL:
            predicate = LLVMIntSGE;
            break;

        case AST_OP_LESS_EQUAL:
            predicate = LLVMIntSLE;
            break;

        default:
            return NULL;
    }

    return LLVMBuildICmp(
        compiler->builder,
        predicate,
        cmp,
        zero,
        "cad.orden"
    );
}


LLVMValueRef llvm_build_comparison(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef left,
    LLVMValueRef right
)
{
    if (compiler == NULL ||
        left == NULL ||
        right == NULL)
        return NULL;

    llvm_balance_numeric(
        compiler,
        &left,
        &right
    );

    LLVMTypeKind kind =
        LLVMGetTypeKind(
            LLVMTypeOf(left)
        );

    /*
     * En la semántica actual un puntero en una comparación
     * solo puede ser una cadena (listas y maps son structs).
     * Soporta ==, !=, <, >, <=, >= (0.3.0).
     */
    if (kind == LLVMPointerTypeKind)
    {
        return llvm_build_string_compare(
            compiler,
            operator,
            left,
            right
        );
    }

    int is_float =
        kind == LLVMDoubleTypeKind ||
        kind == LLVMFloatTypeKind;

    if (is_float) {

        LLVMRealPredicate predicate;

        switch (operator) {

            case AST_OP_EQUAL:
                predicate = LLVMRealOEQ;
                break;

            case AST_OP_NOT_EQUAL:
                predicate = LLVMRealONE;
                break;

            case AST_OP_GREATER:
                predicate = LLVMRealOGT;
                break;

            case AST_OP_LESS:
                predicate = LLVMRealOLT;
                break;

            case AST_OP_GREATER_EQUAL:
                predicate = LLVMRealOGE;
                break;

            case AST_OP_LESS_EQUAL:
                predicate = LLVMRealOLE;
                break;

            default:
                return NULL;
        }

        return LLVMBuildFCmp(
            compiler->builder,
            predicate,
            left,
            right,
            "cmp"
        );
    }

    LLVMIntPredicate predicate;

    switch (operator) {

        case AST_OP_EQUAL:
            predicate = LLVMIntEQ;
            break;

        case AST_OP_NOT_EQUAL:
            predicate = LLVMIntNE;
            break;

        case AST_OP_GREATER:
            predicate = LLVMIntSGT;
            break;

        case AST_OP_LESS:
            predicate = LLVMIntSLT;
            break;

        case AST_OP_GREATER_EQUAL:
            predicate = LLVMIntSGE;
            break;

        case AST_OP_LESS_EQUAL:
            predicate = LLVMIntSLE;
            break;

        default:
            return NULL;
    }

    return LLVMBuildICmp(
        compiler->builder,
        predicate,
        left,
        right,
        "cmp"
    );
}


LLVMValueRef llvm_build_unary(
    LLVMCompiler *compiler,
    ASTOperator operator,
    LLVMValueRef operand
)
{
    if (compiler == NULL ||
        operand == NULL)
        return NULL;

    switch (operator) {

        case AST_OP_NEGATE:

        {
            LLVMTypeKind kind =
                LLVMGetTypeKind(
                    LLVMTypeOf(operand)
                );

            if (kind == LLVMDoubleTypeKind ||
                kind == LLVMFloatTypeKind) {

                return LLVMBuildFNeg(
                    compiler->builder,
                    operand,
                    "neg"
                );
            }

            return LLVMBuildNeg(
                compiler->builder,
                operand,
                "neg"
            );
        }


        case AST_OP_NOT:

            return LLVMBuildNot(
                compiler->builder,
                operand,
                "not"
            );


        default:
            return NULL;
    }
}


/* =========================================================
 * GENERACIÓN DE VARIABLES
 * ========================================================= */

int llvm_generate_var(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return 0;

    LLVMTypeRef type =
        llvm_type(
            compiler,
            node->data.var_decl.type
        );

    LLVMValueRef variable =
        llvm_create_variable(
            compiler,
            node->data.var_decl.name,
            type
        );

    if (variable == NULL)
        return 0;

    LLVMValueRef value =
        llvm_generate_expression(
            compiler,
            node->data.var_decl.value
        );

    if (value == NULL)
        return 0;

    return llvm_store_variable(
        compiler,
        variable,
        value
    );
}


int llvm_generate_const(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return 0;

    LLVMTypeRef type =
        llvm_type(
            compiler,
            node->data.const_decl.type
        );

    LLVMValueRef variable =
        llvm_create_variable(
            compiler,
            node->data.const_decl.name,
            type
        );

    if (variable == NULL)
        return 0;

    LLVMValueRef value =
        llvm_generate_expression(
            compiler,
            node->data.const_decl.value
        );

    if (value == NULL)
        return 0;

    return llvm_store_variable(
        compiler,
        variable,
        value
    );
}


/* =========================================================
 * IDENTIFICADORES
 * ========================================================= */

LLVMValueRef llvm_generate_identifier(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    LLVMValueRef variable =
        llvm_find_variable(
            compiler,
            node->data.identifier.name
        );

    if (variable == NULL) {

        /*
         * Puede ser una función.
         */
        LLVMValueRef function =
            llvm_find_function(
                compiler,
                node->data.identifier.name
            );

        return function;
    }

    return llvm_load_variable(
        compiler,
        variable
    );
}


/* =========================================================
 * BINARY
 * ========================================================= */

LLVMValueRef llvm_generate_binary(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    LLVMValueRef left =
        llvm_generate_expression(
            compiler,
            node->data.binary.left
        );

    LLVMValueRef right =
        llvm_generate_expression(
            compiler,
            node->data.binary.right
        );

    if (left == NULL ||
        right == NULL)
        return NULL;

    ASTOperator operator =
        node->data.binary.operator;

    Type *left_type =
        node->data.binary.left->expr_type;

    if (left_type != NULL &&
        left_type->kind == TYPE_STRUCT &&
        (operator == AST_OP_EQUAL ||
         operator == AST_OP_NOT_EQUAL)) {
        return LLVMBuildICmp(
            compiler->builder,
            operator == AST_OP_EQUAL
                ? LLVMIntEQ
                : LLVMIntNE,
            left,
            right,
            "struct.identity"
        );
    }

    /*
     * opaque == null / opaque == opaque: comparación por
     * identidad de puntero, no por contenido de cadena.
     */
    Type *right_type =
        node->data.binary.right->expr_type;

    if ((left_type != NULL &&
         left_type->kind == TYPE_OPAQUE) ||
        (right_type != NULL &&
         right_type->kind == TYPE_OPAQUE)) {

        if (operator == AST_OP_EQUAL ||
            operator == AST_OP_NOT_EQUAL) {

            return LLVMBuildICmp(
                compiler->builder,
                operator == AST_OP_EQUAL
                    ? LLVMIntEQ
                    : LLVMIntNE,
                left,
                right,
                "opaque.identity"
            );
        }
    }

    switch (operator) {

        case AST_OP_EQUAL:
        case AST_OP_NOT_EQUAL:
        case AST_OP_GREATER:
        case AST_OP_LESS:
        case AST_OP_GREATER_EQUAL:
        case AST_OP_LESS_EQUAL:

            return llvm_build_comparison(
                compiler,
                operator,
                left,
                right
            );

        default:

            return llvm_build_binary(
                compiler,
                operator,
                left,
                right,
                node->expr_type
            );
    }
}


/* =========================================================
 * UNARY
 * ========================================================= */

LLVMValueRef llvm_generate_unary(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (node->data.unary.operator == AST_OP_INCREMENT ||
        node->data.unary.operator == AST_OP_DECREMENT) {
        ASTNode *identifier = node->data.unary.operand;
        LLVMValueRef variable =
            llvm_find_variable(
                compiler,
                identifier->data.identifier.name
            );

        if (variable == NULL)
            return NULL;

        LLVMValueRef previous =
            llvm_load_variable(compiler, variable);

        if (previous == NULL)
            return NULL;

        LLVMValueRef one =
            llvm_generate_int(compiler, 1);

        if (one == NULL)
            return NULL;

        LLVMValueRef updated =
            node->data.unary.operator == AST_OP_INCREMENT
                ? llvm_build_checked_int_op(
                    compiler,
                    "rivo_add_int",
                    previous,
                    one,
                    "increment"
                )
                : llvm_build_checked_int_op(
                    compiler,
                    "rivo_sub_int",
                    previous,
                    one,
                    "decrement"
                );

        if (updated == NULL)
            return NULL;

        if (!llvm_store_variable(compiler, variable, updated))
            return NULL;

        return node->data.unary.is_postfix
            ? previous
            : updated;
    }

    LLVMValueRef operand =
        llvm_generate_expression(
            compiler,
            node->data.unary.operand
        );

    if (operand == NULL)
        return NULL;

    return llvm_build_unary(
        compiler,
        node->data.unary.operator,
        operand
    );
}


/* =========================================================
 * ASIGNACIÓN
 * ======================================================= */

/*
 * Operador binario equivalente a una asignación compuesta.
 *
 * Si el operador no es compuesto devuelve AST_OP_ASSIGN,
 * que el llamante distingue antes de llegar hasta aquí.
 */
static ASTOperator llvm_compound_binary_operator(
    ASTOperator operator
)
{
    switch (operator) {

        case AST_OP_ADD_ASSIGN:
            return AST_OP_ADD;

        case AST_OP_SUB_ASSIGN:
            return AST_OP_SUB;

        case AST_OP_MUL_ASSIGN:
            return AST_OP_MUL;

        case AST_OP_DIV_ASSIGN:
            return AST_OP_DIV;

        default:
            return AST_OP_ASSIGN;
    }
}


/*
 * Asignación a un elemento indexado:
 *
 *     numeros[0] = 10
 *     edades["Ana"] = 21
 *     numeros[0] += 1
 *
 * La escritura va al runtime, que comprueba el rango
 * (decisión de diseño 2026-10-06).
 */
static LLVMValueRef llvm_generate_index_assignment(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef value
)
{
    ASTNode *target =
        node->data.assignment.target;

    Type *object_type =
        (target->data.index.object != NULL)
            ? target->data.index.object->expr_type
            : NULL;

    if (object_type == NULL ||
        (object_type->kind != TYPE_LIST &&
         object_type->kind != TYPE_MAP)) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "el destino no se puede indexar"
        );

        return NULL;
    }

    LLVMValueRef object =
        llvm_generate_expression(
            compiler,
            target->data.index.object
        );

    LLVMValueRef key =
        llvm_generate_expression(
            compiler,
            target->data.index.index
        );

    if (object == NULL ||
        key == NULL)
        return NULL;

    LLVMValueRef key_bits =
        llvm_value_to_i64(
            compiler,
            key,
            node->line,
            node->column
        );

    if (key_bits == NULL)
        return NULL;

    ASTOperator compound =
        llvm_compound_binary_operator(
            node->data.assignment.operator
        );

    if (compound != AST_OP_ASSIGN) {

        /*
         * Necesito el valor actual para calcular el
         * resultado: misma llamada de lectura que
         * llvm_generate_index().
         */
        const Type *element_type =
            object_type->kind == TYPE_LIST
                ? object_type->element_type
                : object_type->value_type;

        LLVMTypeRef pointer_type =
            llvm_opaque_pointer(compiler);

        LLVMTypeRef int_type =
            llvm_int_type(compiler);

        LLVMTypeRef read_parameters[] = {
            pointer_type,
            int_type,
            int_type
        };

        LLVMValueRef read_arguments[] = {
            object,
            key_bits,
            llvm_generate_int(compiler, node->line)
        };

        LLVMValueRef old_bits =
            llvm_call_runtime(
                compiler,
                object_type->kind == TYPE_LIST
                    ? "rivo_list_get"
                    : "rivo_map_get",
                int_type,
                read_parameters,
                3,
                read_arguments
            );

        if (old_bits == NULL)
            return NULL;

        LLVMValueRef old_value =
            llvm_value_from_i64(
                compiler,
                old_bits,
                element_type,
                node->line,
                node->column
            );

        if (old_value == NULL)
            return NULL;

        value =
            llvm_build_binary(
                compiler,
                compound,
                old_value,
                value,
                element_type
            );

        if (value == NULL)
            return NULL;
    }

    LLVMValueRef value_bits =
        llvm_value_to_i64(
            compiler,
            value,
            node->line,
            node->column
        );

    if (value_bits == NULL)
        return NULL;

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMTypeRef void_type =
        LLVMVoidTypeInContext(compiler->context);

    if (object_type->kind == TYPE_LIST) {

        /*
         * rivo_list_set recibe la línea original: es quien
         * comprueba el rango y reporta el error.
         */
        LLVMTypeRef write_parameters[] = {
            pointer_type,
            int_type,
            int_type,
            int_type
        };

        LLVMValueRef write_arguments[] = {
            object,
            key_bits,
            value_bits,
            llvm_generate_int(compiler, node->line)
        };

        if (llvm_call_runtime(
                compiler,
                "rivo_list_set",
                void_type,
                write_parameters,
                4,
                write_arguments
            ) == NULL) {

            return NULL;
        }

    } else {

        LLVMTypeRef write_parameters[] = {
            pointer_type,
            int_type,
            int_type
        };

        LLVMValueRef write_arguments[] = {
            object,
            key_bits,
            value_bits
        };

        if (llvm_call_runtime(
                compiler,
                "rivo_map_set",
                void_type,
                write_parameters,
                3,
                write_arguments
            ) == NULL) {

            return NULL;
        }
    }

    return value;
}


static LLVMValueRef llvm_generate_member_address(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL ||
        node->kind != AST_MEMBER)
        return NULL;

    ASTNode *object_node =
        node->data.member.object;

    Type *object_type =
        object_node != NULL
            ? object_node->expr_type
            : NULL;

    if (object_type == NULL ||
        object_type->kind != TYPE_STRUCT) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "solo los structs tienen campos asignables"
        );
        return NULL;
    }

    LLVMStructInfo *info =
        llvm_find_struct(
            compiler,
            object_type->struct_name
        );

    if (info == NULL) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "no se encontro la definicion LLVM del struct"
        );
        return NULL;
    }

    ASTNodeList *fields =
        &info->declaration->data.struct_decl.fields;

    int field_index = -1;

    for (int i = 0; i < fields->count; i++) {
        ASTNode *field = fields->items[i];

        if (field != NULL &&
            field->data.var_decl.name != NULL &&
            node->data.member.member != NULL &&
            strcmp(
                field->data.var_decl.name,
                node->data.member.member
            ) == 0) {
            field_index = i;
            break;
        }
    }

    if (field_index < 0) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_AST,
            node->line,
            node->column,
            "no se encontro el campo del struct"
        );
        return NULL;
    }

    LLVMValueRef object =
        llvm_generate_expression(
            compiler,
            object_node
        );

    if (object == NULL)
        return NULL;

    return LLVMBuildStructGEP2(
        compiler->builder,
        info->type,
        object,
        (unsigned) field_index,
        "struct.field.addr"
    );
}


LLVMValueRef llvm_generate_assignment(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    ASTNode *target =
        node->data.assignment.target;

    LLVMValueRef value =
        llvm_generate_expression(
            compiler,
            node->data.assignment.value
        );

    if (value == NULL)
        return NULL;

    if (target->kind == AST_INDEX)
        return llvm_generate_index_assignment(
            compiler,
            node,
            value
        );

    if (target->kind == AST_MEMBER) {
        LLVMValueRef address =
            llvm_generate_member_address(
                compiler,
                target
            );

        if (address == NULL)
            return NULL;

        ASTOperator operator =
            node->data.assignment.operator;

        if (operator != AST_OP_ASSIGN) {
            LLVMTypeRef field_type =
                llvm_type(
                    compiler,
                    target->expr_type
                );

            LLVMValueRef old_value =
                LLVMBuildLoad2(
                    compiler->builder,
                    field_type,
                    address,
                    "struct.field"
                );

            value = llvm_build_binary(
                compiler,
                llvm_compound_binary_operator(operator),
                old_value,
                value,
                target->expr_type
            );

            if (value == NULL)
                return NULL;
        }

        LLVMBuildStore(
            compiler->builder,
            value,
            address
        );

        return value;
    }

    if (target->kind != AST_IDENTIFIER) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "el destino no puede recibir una asignacion"
        );

        return NULL;
    }

    LLVMValueRef variable =
        llvm_find_variable(
            compiler,
            target->data.identifier.name
        );

    if (variable == NULL)
        return NULL;

    ASTOperator operator =
        node->data.assignment.operator;

    if (operator != AST_OP_ASSIGN) {

        LLVMValueRef old_value =
            llvm_load_variable(
                compiler,
                variable
            );

        ASTOperator binary_operator =
            llvm_compound_binary_operator(operator);

        value =
            llvm_build_binary(
                compiler,
                binary_operator,
                old_value,
                value,
                node->data.assignment.value->expr_type
            );

        if (value == NULL)
            return NULL;
    }

    llvm_store_variable(
        compiler,
        variable,
        value
    );

    return value;
}


/* =========================================================
 * FUNCIONES DEL RUNTIME (EXTERNAS)
 *
 * El runtime de E++ se compila aparte (src/runtime.c) y se
 * enlaza con los programas generados. Aquí solo se declaran
 * como externas en el módulo.
 *
 * Mapeo (claude.md §24):
 *
 *     print(int)      -> void rivo_print_int(i64)
 *     print(float)  -> void rivo_print_float(double)
 *     print(texto)    -> void rivo_print_text(i8*)
 *     print(booleano) -> void rivo_print_bool(i8)
 *     print(char) -> void rivo_print_char(i8)
 *     input()         -> i8* rivo_input()
 *     len(texto)      -> i64 rivo_len_text(i8*)
 *     string + string -> i8* rivo_concat_string(i8*, i8*)
 * ======================================================= */

static LLVMValueRef llvm_add_runtime_function(
    LLVMCompiler *compiler,
    const char *name,
    LLVMTypeRef return_type,
    LLVMTypeRef *parameter_types,
    int parameter_count
)
{
    LLVMValueRef function =
        LLVMGetNamedFunction(
            compiler->module,
            name
        );

    if (function != NULL)
        return function;

    LLVMTypeRef function_type =
        LLVMFunctionType(
            return_type,
            parameter_types,
            parameter_count,
            0
        );

    return LLVMAddFunction(
        compiler->module,
        name,
        function_type
    );
}


/* =========================================================
 * FUNCIONES INCORPORADAS (include/builtins.h)
 *
 * La tabla la comparte la semántica, así que el número y
 * el tipo de argumentos no pueden divergir entre fases.
 * ======================================================= */

/*
 * Tipo LLVM de una entrada de la tabla.
 *
 * bool y char miden 8 bits en el runtime (uint8_t y
 * char); la reducción a i1 se hace después de la llamada.
 */
static LLVMTypeRef llvm_builtin_type(
    LLVMCompiler *compiler,
    TypeKind kind
)
{
    switch (kind) {

        case TYPE_INT:
            return LLVMInt64TypeInContext(
                compiler->context
            );

        case TYPE_FLOAT:
            return LLVMDoubleTypeInContext(
                compiler->context
            );

        case TYPE_BOOL:
        case TYPE_CHAR:
            return LLVMInt8TypeInContext(
                compiler->context
            );

        case TYPE_STRING:
        case TYPE_LIST:
        case TYPE_MAP:
            return LLVMPointerType(
                LLVMInt8TypeInContext(compiler->context),
                0
            );

        default:
            return NULL;
    }
}


/*
 * Declara `rivo_<nombre>` con la firma de la tabla.
 *
 * Devuelve NULL sin reportar error si el nombre no está en
 * la tabla: el llamante continúa con el resto de rutas.
 */
static LLVMValueRef llvm_declare_builtin_call(
    LLVMCompiler *compiler,
    const char *name
)
{
    const RivoBuiltin *builtin =
        rivo_builtin_find(name);

    if (builtin == NULL)
        return NULL;

    LLVMTypeRef parameter_types[3] = {
        NULL, NULL, NULL
    };

    for (int i = 0; i < builtin->argument_count; i++) {

        parameter_types[i] =
            llvm_builtin_type(
                compiler,
                builtin->arguments[i]
            );

        if (parameter_types[i] == NULL)
            return NULL;
    }

    LLVMTypeRef return_type =
        builtin->result == TYPE_NULL
            ? LLVMVoidTypeInContext(compiler->context)
            : llvm_builtin_type(
                compiler,
                builtin->result
              );

    if (return_type == NULL)
        return NULL;

    char runtime_name[64];

    /*
     * concat: el nombre runtime es rivo_concat_string, no
     * rivo_concat (include/runtime.h). El resto usa la
     * regla rivo_<nombre>.
     */
    if (strcmp(builtin->name, "concat") == 0) {

        snprintf(
            runtime_name,
            sizeof(runtime_name),
            "rivo_concat_string"
        );

    } else {

        snprintf(
            runtime_name,
            sizeof(runtime_name),
            "rivo_%s",
            builtin->name
        );
    }

    return llvm_add_runtime_function(
        compiler,
        runtime_name,
        return_type,
        parameter_types,
        builtin->argument_count
    );
}


/*
 * Resuelve la función runtime que implementa una llamada.
 *
 * `arguments` puede modificarse (ampliación de bool
 * de i1 a i8) para que coincida con la firma del runtime.
 *
 * `first_argument_type` es el tipo estático del primer
 * argumento (anotado por la semántica): hace falta para
 * distinguir len(string) de len(list<T>), porque ambos
 * se representan como puntero.
 *
 * Si el nombre no es una función runtime conocida devuelve
 * NULL sin reportar error. Si es conocida pero no se puede
 * generar, reporta un error de codegen y devuelve NULL.
 */
static LLVMValueRef llvm_declare_runtime_call(
    LLVMCompiler *compiler,
    const char *name,
    LLVMValueRef *arguments,
    int argument_count,
    const Type *first_argument_type,
    int line,
    int column
)
{
    if (name == NULL)
        return NULL;

    LLVMContextRef context = compiler->context;

    LLVMTypeRef pointer_type =
        LLVMPointerType(
            LLVMInt8TypeInContext(context),
            0
        );


    /* input([prompt]) -> string */

    if (strcmp(name, "input") == 0) {

        if (argument_count == 0) {
            return llvm_add_runtime_function(
                compiler,
                "rivo_input",
                pointer_type,
                NULL,
                0
            );
        }

        if (argument_count == 1 &&
            arguments != NULL) {
            LLVMTypeRef parameter_types[1] = {
                pointer_type
            };

            return llvm_add_runtime_function(
                compiler,
                "rivo_input_prompt",
                pointer_type,
                parameter_types,
                1
            );
        }

        {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "input() acepta cero o un argumento string"
            );

            return NULL;
        }
    }


    /* str(valor) -> string */

    if (strcmp(name, "str") == 0) {

        if (argument_count != 1 ||
            arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "str() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef argument_type =
            LLVMTypeOf(arguments[0]);

        const char *runtime_name = NULL;
        LLVMTypeRef parameter_type = argument_type;

        switch (LLVMGetTypeKind(argument_type)) {

            case LLVMPointerTypeKind:

                runtime_name = "rivo_str_from_string";
                parameter_type = pointer_type;
                break;

            case LLVMIntegerTypeKind:
            {
                unsigned width =
                    LLVMGetIntTypeWidth(argument_type);

                if (width == 64) {

                    runtime_name = "rivo_str_from_int";
                    parameter_type =
                        LLVMInt64TypeInContext(context);

                } else if (width == 1) {

                    /*
                     * bool es i1 en E++ y uint8_t en
                     * el runtime.
                     */
                    runtime_name =
                        "rivo_str_from_bool";
                    parameter_type =
                        LLVMInt8TypeInContext(context);

                    arguments[0] =
                        LLVMBuildZExt(
                            compiler->builder,
                            arguments[0],
                            parameter_type,
                            "rivo_bool"
                        );

                } else if (width == 8) {

                    runtime_name =
                        "rivo_str_from_char";
                    parameter_type =
                        LLVMInt8TypeInContext(context);
                }

                break;
            }

            case LLVMDoubleTypeKind:

                runtime_name =
                    "rivo_str_from_float";
                parameter_type =
                    LLVMDoubleTypeInContext(context);
                break;

            default:
                break;
        }

        if (runtime_name == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo no soportado por str()"
            );

            return NULL;
        }

        return llvm_add_runtime_function(
            compiler,
            runtime_name,
            pointer_type,
            &parameter_type,
            1
        );
    }


    /* range(a, b) -> list<int> */

    if (strcmp(name, "range") == 0) {

        if (argument_count != 2 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "range() recibe exactamente 2 argumentos"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = {
            LLVMInt64TypeInContext(context),
            LLVMInt64TypeInContext(context)
        };

        return llvm_add_runtime_function(
            compiler,
            "rivo_range",
            pointer_type,
            parameters,
            2
        );
    }


    /* divmod(a, b) -> list<int> */

    if (strcmp(name, "divmod") == 0) {

        if (argument_count != 2 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "divmod() recibe exactamente 2 argumentos"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = {
            LLVMInt64TypeInContext(context),
            LLVMInt64TypeInContext(context)
        };

        return llvm_add_runtime_function(
            compiler,
            "rivo_divmod",
            pointer_type,
            parameters,
            2
        );
    }


    /* sorted(lista) -> list */

    if (strcmp(name, "sorted") == 0) {

        if (argument_count != 1 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "sorted() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = { pointer_type };

        return llvm_add_runtime_function(
            compiler,
            "rivo_sorted",
            pointer_type,
            parameters,
            1
        );
    }


    /* reversed(lista) -> list */

    if (strcmp(name, "reversed") == 0) {

        if (argument_count != 1 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "reversed() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = { pointer_type };

        return llvm_add_runtime_function(
            compiler,
            "rivo_reversed",
            pointer_type,
            parameters,
            1
        );
    }


    /* all(lista) -> bool */

    if (strcmp(name, "all") == 0) {

        if (argument_count != 1 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "all() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = { pointer_type };

        return llvm_add_runtime_function(
            compiler,
            "rivo_all",
            LLVMInt8TypeInContext(context),
            parameters,
            1
        );
    }


    /* any(lista) -> bool */

    if (strcmp(name, "any") == 0) {

        if (argument_count != 1 || arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "any() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = { pointer_type };

        return llvm_add_runtime_function(
            compiler,
            "rivo_any",
            LLVMInt8TypeInContext(context),
            parameters,
            1
        );
    }


    /* len(valor) -> int */

    if (strcmp(name, "len") == 0) {

        if (argument_count != 1 ||
            arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "len() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        /*
         * len(list<T>): la lista es un puntero opaco, igual
         * que un string, así que el desambiguador es el tipo
         * estático del argumento.
         */
        if (first_argument_type != NULL &&
            first_argument_type->kind == TYPE_LIST) {

            LLVMTypeRef list_parameter = pointer_type;

            return llvm_add_runtime_function(
                compiler,
                "rivo_list_len",
                LLVMInt64TypeInContext(context),
                &list_parameter,
                1
            );
        }

        /*
         * len(map<K,V>): decisión de diseño 2026-10-06.
         * Mismo desambiguador por tipo estático.
         */
        if (first_argument_type != NULL &&
            first_argument_type->kind == TYPE_MAP) {

            LLVMTypeRef map_parameter = pointer_type;

            return llvm_add_runtime_function(
                compiler,
                "rivo_map_len",
                LLVMInt64TypeInContext(context),
                &map_parameter,
                1
            );
        }

        LLVMTypeRef argument_type =
            LLVMTypeOf(arguments[0]);

        if (LLVMGetTypeKind(argument_type) !=
            LLVMPointerTypeKind) {

            /*
             * El análisis semántico solo admite string
             * (i8*); cualquier otro tipo llegado hasta
             * aquí no tiene representación en runtime.
             */
            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo no soportado por len()"
            );

            return NULL;
        }

        LLVMTypeRef parameter_type = argument_type;

        return llvm_add_runtime_function(
            compiler,
            "rivo_len_text",
            LLVMInt64TypeInContext(context),
            &parameter_type,
            1
        );
    }


    /* print(valor) -> sin valor */

    if (strcmp(name, "print") == 0) {

        if (argument_count != 1 ||
            arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                line,
                column,
                "print() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMTypeRef argument_type =
            LLVMTypeOf(arguments[0]);

        LLVMTypeRef parameter_type = argument_type;
        const char *runtime_name = NULL;

        switch (LLVMGetTypeKind(argument_type)) {

            case LLVMPointerTypeKind:

                runtime_name = "rivo_print_text";
                parameter_type = pointer_type;
                break;

            case LLVMIntegerTypeKind:
            {
                unsigned width =
                    LLVMGetIntTypeWidth(argument_type);

                if (width == 64) {

                    runtime_name = "rivo_print_int";
                    parameter_type =
                        LLVMInt64TypeInContext(context);

                } else if (width == 1) {

                    /*
                     * bool es i1 en E++ y uint8_t en
                     * el runtime.
                     */
                    runtime_name =
                        "rivo_print_bool";
                    parameter_type =
                        LLVMInt8TypeInContext(context);

                    arguments[0] =
                        LLVMBuildZExt(
                            compiler->builder,
                            arguments[0],
                            parameter_type,
                            "rivo_bool"
                        );

                } else if (width == 8) {

                    runtime_name =
                        "rivo_print_char";
                    parameter_type =
                        LLVMInt8TypeInContext(context);
                }

                break;
            }

            case LLVMDoubleTypeKind:

                runtime_name = "rivo_print_float";
                parameter_type =
                    LLVMDoubleTypeInContext(context);
                break;

            default:
                break;
        }

        if (runtime_name == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo no soportado por print()"
            );

            return NULL;
        }

        return llvm_add_runtime_function(
            compiler,
            runtime_name,
            LLVMVoidTypeInContext(context),
            &parameter_type,
            1
        );
    }

    return NULL;
}


/*
 * Llamada a un método de lista: lista.add(v),
 * lista.remove(v), lista.len() (claude.md §11).
 *
 * El objeto ya está generado (`object`) y también los
 * argumentos (`arguments`).
 */
static LLVMValueRef llvm_generate_collection_method_call(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef object,
    LLVMValueRef *arguments,
    int argument_count
)
{
    const char *member =
        node->data.call.callee->data.member.member;

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMTypeRef void_type =
        LLVMVoidTypeInContext(compiler->context);

    Type *object_type =
        (node->data.call.callee->data.member.object != NULL)
            ? node->data.call.callee->data.member.object->expr_type
            : NULL;

    /*
     * map: mapa.remove(clave) y mapa.len() (0.3.0:
     * len unificado con lista.len()).
     */
    if (object_type != NULL &&
        object_type->kind == TYPE_MAP) {

        if (member != NULL &&
            strcmp(member, "len") == 0) {

            if (argument_count != 0) {

                llvm_report_error(
                    compiler,
                    ERROR_CODEGEN_INVALID_OPERATION,
                    node->line,
                    node->column,
                    "mapa.len() no recibe argumentos"
                );

                return NULL;
            }

            LLVMTypeRef map_len_params[] = {
                pointer_type
            };

            LLVMValueRef map_len_args[] = {
                object
            };

            return llvm_call_runtime(
                compiler,
                "rivo_map_len",
                int_type,
                map_len_params,
                1,
                map_len_args
            );
        }

        if (member == NULL ||
            strcmp(member, "remove") != 0) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                node->line,
                node->column,
                "miembro desconocido de map"
            );

            return NULL;
        }

        if (argument_count != 1 ||
            arguments == NULL) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                node->line,
                node->column,
                "mapa.remove() recibe exactamente 1 argumento"
            );

            return NULL;
        }

        LLVMValueRef key_bits =
            llvm_value_to_i64(
                compiler,
                arguments[0],
                node->line,
                node->column
            );

        if (key_bits == NULL)
            return NULL;

        LLVMTypeRef map_parameters[] = {
            pointer_type,
            int_type
        };

        LLVMValueRef map_arguments[] = {
            object,
            key_bits
        };

        return llvm_call_runtime(
            compiler,
            "rivo_map_remove",
            void_type,
            map_parameters,
            2,
            map_arguments
        );
    }

    if (member != NULL &&
        strcmp(member, "len") == 0) {

        if (argument_count != 0) {

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_OPERATION,
                node->line,
                node->column,
                "lista.len() no recibe argumentos"
            );

            return NULL;
        }

        LLVMTypeRef parameters[] = {
            pointer_type
        };

        LLVMValueRef call_arguments[] = {
            object
        };

        return llvm_call_runtime(
            compiler,
            "rivo_list_len",
            int_type,
            parameters,
            1,
            call_arguments
        );
    }

    int is_add =
        member != NULL && strcmp(member, "add") == 0;

    int is_remove =
        member != NULL && strcmp(member, "remove") == 0;

    if (!is_add && !is_remove) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "miembro desconocido de list"
        );

        return NULL;
    }

    if (argument_count != 1 ||
        arguments == NULL) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "el metodo de lista recibe exactamente 1 argumento"
        );

        return NULL;
    }

    LLVMValueRef value_bits =
        llvm_value_to_i64(
            compiler,
            arguments[0],
            node->line,
            node->column
        );

    if (value_bits == NULL)
        return NULL;

    LLVMTypeRef parameters[] = {
        pointer_type,
        int_type
    };

    LLVMValueRef call_arguments[] = {
        object,
        value_bits
    };

    return llvm_call_runtime(
        compiler,
        is_add ? "rivo_list_add" : "rivo_list_remove",
        void_type,
        parameters,
        2,
        call_arguments
    );
}


static LLVMValueRef llvm_generate_struct_constructor(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef *arguments,
    int argument_count
)
{
    if (compiler == NULL ||
        node == NULL ||
        node->data.call.callee == NULL ||
        node->data.call.callee->kind != AST_IDENTIFIER)
        return NULL;

    const char *name =
        node->data.call.callee->data.identifier.name;

    LLVMStructInfo *info =
        llvm_find_struct(compiler, name);

    if (info == NULL) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "no se encontro la definicion LLVM del constructor"
        );
        return NULL;
    }

    ASTNodeList *fields =
        &info->declaration->data.struct_decl.fields;

    if (fields->count != argument_count) {
        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_AST,
            node->line,
            node->column,
            "cantidad de argumentos invalida para el constructor"
        );
        return NULL;
    }

    LLVMValueRef size =
        LLVMSizeOf(info->type);

    LLVMTypeRef size_type =
        LLVMTypeOf(size);

    unsigned size_width =
        LLVMGetIntTypeWidth(size_type);

    if (size_width < 64) {
        size = LLVMBuildZExt(
            compiler->builder,
            size,
            llvm_int_type(compiler),
            "struct.size"
        );
    } else if (size_width > 64) {
        size = LLVMBuildTrunc(
            compiler->builder,
            size,
            llvm_int_type(compiler),
            "struct.size"
        );
    }

    LLVMTypeRef allocation_parameters[] = {
        llvm_int_type(compiler)
    };

    LLVMValueRef allocation =
        llvm_add_runtime_function(
            compiler,
            "rivo_struct_alloc",
            llvm_opaque_pointer(compiler),
            allocation_parameters,
            1
        );

    if (allocation == NULL) {
        llvm_report_internal(
            compiler,
            "no se pudo declarar rivo_struct_alloc"
        );
        return NULL;
    }

    LLVMValueRef allocated =
        LLVMBuildCall2(
            compiler->builder,
            LLVMGlobalGetValueType(allocation),
            allocation,
            &size,
            1,
            "struct.raw"
        );

    LLVMTypeRef struct_pointer_type =
        LLVMPointerType(info->type, 0);

    LLVMValueRef instance =
        LLVMTypeOf(allocated) == struct_pointer_type
            ? allocated
            : LLVMBuildBitCast(
                  compiler->builder,
                  allocated,
                  struct_pointer_type,
                  "struct.instance"
              );

    for (int i = 0; i < fields->count; i++) {
        LLVMValueRef address =
            LLVMBuildStructGEP2(
                compiler->builder,
                info->type,
                instance,
                (unsigned) i,
                "struct.field.addr"
            );

        LLVMBuildStore(
            compiler->builder,
            arguments[i],
            address
        );
    }

    return instance;
}


static int llvm_emit_inline_text(
    LLVMCompiler *compiler,
    const char *text
)
{
    LLVMValueRef value =
        llvm_generate_string(compiler, text);

    if (value == NULL)
        return 0;

    LLVMTypeRef parameter =
        llvm_text_type(compiler);

    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            "rivo_print_text_inline",
            LLVMVoidTypeInContext(compiler->context),
            &parameter,
            1
        );

    if (function == NULL)
        return 0;

    LLVMBuildCall2(
        compiler->builder,
        LLVMGlobalGetValueType(function),
        function,
        &value,
        1,
        ""
    );

    return 1;
}


static int llvm_emit_inline_value(
    LLVMCompiler *compiler,
    const Type *type,
    LLVMValueRef value,
    int line,
    int column
)
{
    if (type == NULL ||
        value == NULL)
        return 0;

    if (type->kind == TYPE_STRUCT) {
        LLVMStructInfo *info =
            llvm_find_struct(
                compiler,
                type->struct_name
            );

        if (info == NULL) {
            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "tipo de struct sin declaracion LLVM"
            );
            return 0;
        }

        size_t name_length =
            strlen(info->declaration->data.struct_decl.name);

        char *opening =
            malloc(name_length + 2);

        if (opening == NULL)
            return 0;

        snprintf(
            opening,
            name_length + 2,
            "%s(",
            info->declaration->data.struct_decl.name
        );

        int ok = llvm_emit_inline_text(compiler, opening);
        free(opening);

        if (!ok)
            return 0;

        ASTNodeList *fields =
            &info->declaration->data.struct_decl.fields;

        for (int i = 0; i < fields->count; i++) {
            ASTNode *field = fields->items[i];

            if (i > 0 &&
                !llvm_emit_inline_text(compiler, ", "))
                return 0;

            if (!llvm_emit_inline_text(
                    compiler,
                    field->data.var_decl.name
                ) ||
                !llvm_emit_inline_text(compiler, "="))
                return 0;

            LLVMValueRef address =
                LLVMBuildStructGEP2(
                    compiler->builder,
                    info->type,
                    value,
                    (unsigned) i,
                    "struct.print.field.addr"
                );

            LLVMValueRef field_value =
                LLVMBuildLoad2(
                    compiler->builder,
                    llvm_type(
                        compiler,
                        field->data.var_decl.type
                    ),
                    address,
                    "struct.print.field"
                );

            if (!llvm_emit_inline_value(
                    compiler,
                    field->data.var_decl.type,
                    field_value,
                    line,
                    column
                ))
                return 0;
        }

        return llvm_emit_inline_text(compiler, ")");
    }

    const char *runtime_name = NULL;
    LLVMTypeRef parameter_type = NULL;

    switch (type->kind) {
        case TYPE_INT:
            runtime_name = "rivo_print_int_inline";
            parameter_type = llvm_int_type(compiler);
            break;

        case TYPE_FLOAT:
            runtime_name = "rivo_print_float_inline";
            parameter_type = llvm_float_type(compiler);
            break;

        case TYPE_STRING:
            runtime_name = "rivo_print_text_inline";
            parameter_type = llvm_text_type(compiler);
            break;

        case TYPE_BOOL:
            runtime_name = "rivo_print_bool_inline";
            parameter_type = LLVMInt8TypeInContext(compiler->context);
            value = LLVMBuildZExt(
                compiler->builder,
                value,
                parameter_type,
                "struct.print.bool"
            );
            break;

        case TYPE_CHAR:
            runtime_name = "rivo_print_char_inline";
            parameter_type = llvm_char_type(compiler);
            break;

        default:
            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_TYPE,
                line,
                column,
                "print(struct) solo admite campos imprimibles"
            );
            return 0;
    }

    LLVMValueRef function =
        llvm_add_runtime_function(
            compiler,
            runtime_name,
            LLVMVoidTypeInContext(compiler->context),
            &parameter_type,
            1
        );

    if (function == NULL)
        return 0;

    LLVMBuildCall2(
        compiler->builder,
        LLVMGlobalGetValueType(function),
        function,
        &value,
        1,
        ""
    );

    return 1;
}


/* =========================================================
 * CALL
 * ========================================================= */

LLVMValueRef llvm_generate_call(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    ASTNode *callee = node->data.call.callee;

    const char *name = NULL;

    /*
     * Método de colección: numeros.add(1), numeros.len(),
     * edades.remove("Ana").
     *
     * La semántica ya comprobó que el objeto es una lista
     * o un map, así que se resuelve antes que cualquier
     * función que pueda llamarse igual (add, remove, len).
     */
    int is_collection_method = 0;

    LLVMValueRef method_object = NULL;

    if (callee != NULL &&
        callee->kind == AST_MEMBER) {

        Type *object_type =
            (callee->data.member.object != NULL)
                ? callee->data.member.object->expr_type
                : NULL;

        if (object_type != NULL &&
            (object_type->kind == TYPE_LIST ||
             object_type->kind == TYPE_MAP)) {

            is_collection_method = 1;

            method_object =
                llvm_generate_expression(
                    compiler,
                    callee->data.member.object
                );

            if (method_object == NULL)
                return NULL;
        }
    }

    if (callee != NULL &&
        callee->kind == AST_IDENTIFIER) {

        name = callee->data.identifier.name;
    }
    else if (
        callee != NULL &&
        callee->kind == AST_MEMBER)
    {
        /*
         * Llamada calificada: math.suma(...)
         *
         * La semántica ya validó que es una función de
         * módulo: las funciones viven en un espacio plano
         * de nombres.
         */
        name = callee->data.member.member;
    }

    int argument_count =
        node->data.call.arguments.count;

    /*
     * Tipo de función del callee (semántica lo asigna
     * al expr_type del callee). Sirve para detectar
     * parámetros buffer<T> y expandirlos.
     */
    Type *callee_signature = NULL;

    if (callee != NULL &&
        callee->expr_type != NULL &&
        callee->expr_type->kind == TYPE_FUNCTION)
        callee_signature = callee->expr_type;

    LLVMValueRef *arguments = NULL;

    if (argument_count > 0) {

        /*
         * Contar argumentos expandidos: buffer<T> se
         * convierte en dos argumentos (T*, size).
         */
        int expanded_count = 0;

        for (int i = 0; i < argument_count; i++) {

            const Type *param_type =
                (callee_signature != NULL &&
                 i < callee_signature->parameter_count)
                    ? callee_signature->parameter_types[i]
                    : NULL;

            if (param_type != NULL &&
                param_type->kind == TYPE_BUFFER)
                expanded_count += 2;
            else
                expanded_count++;
        }

        arguments =
            calloc(
                expanded_count,
                sizeof(LLVMValueRef)
            );

        if (arguments == NULL)
            return NULL;

        int index = 0;

        for (int i = 0;
             i < argument_count;
             i++) {

            const Type *param_type =
                (callee_signature != NULL &&
                 i < callee_signature->parameter_count)
                    ? callee_signature->parameter_types[i]
                    : NULL;

            LLVMValueRef value =
                llvm_generate_expression(
                    compiler,
                    node->data.call.arguments.items[i]
                );

            if (value == NULL) {

                free(arguments);
                return NULL;
            }

            /*
             * buffer<T>: extraer puntero al backing store
             * y longitud del RivoList. Layout del runtime:
             *   offset 0:  element_tag (i64)
             *   offset 8:  length (i64)
             *   offset 16: capacity (i64)
             *   offset 24: items (i64*)
             */
            if (param_type != NULL &&
                param_type->kind == TYPE_BUFFER) {

                LLVMTypeRef i64_type =
                    llvm_int_type(compiler);

                LLVMTypeRef list_struct =
                    LLVMStructTypeInContext(
                        compiler->context,
                        (LLVMTypeRef[]){
                            i64_type, i64_type,
                            i64_type,
                            llvm_opaque_pointer(compiler)
                        },
                        4,
                        0
                    );

                /* items (offset 3) → primer argumento */
                LLVMValueRef items_ptr =
                    LLVMBuildStructGEP2(
                        compiler->builder,
                        list_struct,
                        value,
                        3,
                        "buf.items.ptr"
                    );

                arguments[index++] =
                    LLVMBuildLoad2(
                        compiler->builder,
                        llvm_opaque_pointer(compiler),
                        items_ptr,
                        "buf.items"
                    );

                /* length (offset 1) → segundo argumento */
                LLVMValueRef length_ptr =
                    LLVMBuildStructGEP2(
                        compiler->builder,
                        list_struct,
                        value,
                        1,
                        "buf.len.ptr"
                    );

                arguments[index++] =
                    LLVMBuildLoad2(
                        compiler->builder,
                        i64_type,
                        length_ptr,
                        "buf.len"
                    );

            } else {

                arguments[index++] = value;
            }
        }

        argument_count = index;
    }

    if (node->data.call.is_struct_constructor) {
        LLVMValueRef result =
            llvm_generate_struct_constructor(
                compiler,
                node,
                arguments,
                argument_count
            );

        free(arguments);
        return result;
    }

    if (name != NULL && strcmp(name, "print") == 0) {
        for (int i = 0; i < argument_count; i++) {
            if (i > 0) {
                LLVMValueRef separator =
                    llvm_add_runtime_function(
                        compiler,
                        "rivo_print_space",
                        LLVMVoidTypeInContext(compiler->context),
                        NULL,
                        0
                    );
                if (separator == NULL) {
                    free(arguments);
                    llvm_report_internal(
                        compiler,
                        "no se pudo declarar rivo_print_space"
                    );
                    return NULL;
                }
                LLVMBuildCall2(
                    compiler->builder,
                    LLVMGlobalGetValueType(separator),
                    separator,
                    NULL,
                    0,
                    ""
                );
            }

            Type *argument_type =
                node->data.call.arguments.items[i]->expr_type;
            if (argument_type == NULL) {
                free(arguments);
                llvm_report_internal(
                    compiler,
                    "print() recibio un argumento sin tipo"
                );
                return NULL;
            }

            if (argument_type->kind == TYPE_STRUCT) {
                if (!llvm_emit_inline_value(
                        compiler,
                        argument_type,
                        arguments[i],
                        node->line,
                        node->column
                    )) {
                    free(arguments);
                    return NULL;
                }
                continue;
            }

            const char *runtime_name = NULL;
            LLVMTypeRef parameter_type = NULL;

            switch (argument_type->kind) {
                case TYPE_INT:
                    runtime_name = "rivo_print_int_inline";
                    parameter_type =
                        LLVMInt64TypeInContext(compiler->context);
                    break;
                case TYPE_FLOAT:
                    runtime_name = "rivo_print_float_inline";
                    parameter_type =
                        LLVMDoubleTypeInContext(compiler->context);
                    break;
                case TYPE_STRING:
                    runtime_name = "rivo_print_text_inline";
                    parameter_type = llvm_text_type(compiler);
                    break;
                case TYPE_BOOL:
                    runtime_name = "rivo_print_bool_inline";
                    parameter_type =
                        LLVMInt8TypeInContext(compiler->context);
                    arguments[i] =
                        LLVMBuildZExt(
                            compiler->builder,
                            arguments[i],
                            parameter_type,
                            "print.bool"
                        );
                    break;
                case TYPE_CHAR:
                    runtime_name = "rivo_print_char_inline";
                    parameter_type =
                        LLVMInt8TypeInContext(compiler->context);
                    break;
                default:
                    free(arguments);
                    llvm_report_error(
                        compiler,
                        ERROR_CODEGEN_INVALID_TYPE,
                        node->line,
                        node->column,
                        "tipo no soportado por print()"
                    );
                    return NULL;
            }

            LLVMValueRef function =
                llvm_add_runtime_function(
                    compiler,
                    runtime_name,
                    LLVMVoidTypeInContext(compiler->context),
                    &parameter_type,
                    1
                );
            if (function == NULL) {
                free(arguments);
                llvm_report_internal(
                    compiler,
                    "no se pudo declarar la funcion de impresion"
                );
                return NULL;
            }
            LLVMBuildCall2(
                compiler->builder,
                LLVMGlobalGetValueType(function),
                function,
                &arguments[i],
                1,
                ""
            );
        }

        LLVMValueRef newline =
            llvm_add_runtime_function(
                compiler,
                "rivo_print_newline",
                LLVMVoidTypeInContext(compiler->context),
                NULL,
                0
            );
        if (newline == NULL) {
            free(arguments);
            llvm_report_internal(
                compiler,
                "no se pudo declarar rivo_print_newline"
            );
            return NULL;
        }
        LLVMValueRef result =
            LLVMBuildCall2(
                compiler->builder,
                LLVMGlobalGetValueType(newline),
                newline,
                NULL,
                0,
                ""
            );
        free(arguments);
        return result;
    }

    if (callee != NULL &&
        callee->kind == AST_IDENTIFIER &&
        callee->expr_type != NULL &&
        callee->expr_type->kind == TYPE_FUNCTION) {
        LLVMValueRef variable =
            llvm_find_variable(
                compiler,
                callee->data.identifier.name
            );

        if (variable != NULL) {
            LLVMValueRef function_pointer =
                llvm_load_variable(compiler, variable);
            Type *signature = callee->expr_type;
            LLVMTypeRef *parameter_types = NULL;

            if (signature->parameter_count > 0) {
                parameter_types =
                    calloc((size_t) signature->parameter_count,
                           sizeof(LLVMTypeRef));

                if (parameter_types == NULL) {
                    free(arguments);
                    return NULL;
                }

                for (int i = 0;
                     i < signature->parameter_count;
                     i++)
                    parameter_types[i] =
                        llvm_type(compiler,
                                  signature->parameter_types[i]);
            }

            LLVMTypeRef return_type =
                signature->return_type != NULL
                    ? llvm_type(compiler, signature->return_type)
                    : LLVMVoidTypeInContext(compiler->context);

            LLVMTypeRef function_type =
                LLVMFunctionType(
                    return_type,
                    parameter_types,
                    (unsigned) signature->parameter_count,
                    0
                );
            free(parameter_types);

            if (function_pointer == NULL ||
                function_type == NULL) {
                free(arguments);
                return NULL;
            }

            LLVMValueRef result =
                LLVMBuildCall2(
                    compiler->builder,
                    function_type,
                    function_pointer,
                    arguments,
                    (unsigned) argument_count,
                    return_type == LLVMVoidTypeInContext(compiler->context)
                        ? ""
                        : "indirect.call"
                );
            free(arguments);

            return result;
        }
    }

    if (is_collection_method) {

        LLVMValueRef result =
            llvm_generate_collection_method_call(
                compiler,
                node,
                method_object,
                arguments,
                argument_count
            );

        free(arguments);

        return result;
    }

    LLVMValueRef function = NULL;

    if (name != NULL) {

        /*
         * La semántica resolvió la llamada como builtin
         * (join, upper...): no buscar entre las funciones
         * de usuario, porque una de un módulo con el mismo
         * nombre no debe eclipsarla en el espacio plano.
         */
        if (node->data.call.is_builtin) {

            function =
                llvm_declare_builtin_call(
                    compiler,
                    name
                );

        } else {

            function =
                llvm_find_function(compiler, name);

            /*
             * Función incorporada de include/builtins.h:
             * random, upper, read_file, clear...
             */
            if (function == NULL) {

                function =
                    llvm_declare_builtin_call(
                        compiler,
                        name
                    );
            }
        }

        if (function == NULL) {

            function =
                llvm_declare_runtime_call(
                    compiler,
                    name,
                    arguments,
                    argument_count,
                    (argument_count > 0)
                        ? node->data.call.arguments
                              .items[0]->expr_type
                        : NULL,
                    node->line,
                    node->column
                );
        }
    }

    if (function == NULL) {

        free(arguments);
        return NULL;
    }

    /*
     * Con punteros opacos (LLVM 15+) el tipo del valor
     * función es `ptr`: hay que pedir el tipo de función
     * con LLVMGlobalGetValueType.
     */
    LLVMTypeRef function_type =
        LLVMGlobalGetValueType(function);

    /*
     * Coerción de argumentos a los tipos de parámetro de
     * la función (necesario para FFI de ancho fijo: un
     * int E++ (i64) pasa a i8/i16/i32 con trunc).
     *
     * Para funciones variadic, solo se coaccionan los
     * parámetros fijos; los extras se pasan tal cual.
     */
    if (argument_count > 0 &&
        LLVMGetTypeKind(function_type) ==
            LLVMFunctionTypeKind) {

        unsigned param_count =
            LLVMCountParamTypes(function_type);

        int is_variadic =
            LLVMIsFunctionVarArg(function_type);

        if (is_variadic ||
            param_count == (unsigned) argument_count) {

            unsigned coerce_count =
                is_variadic
                    ? (param_count < (unsigned) argument_count
                        ? param_count
                        : (unsigned) argument_count)
                    : param_count;

            LLVMTypeRef *param_types =
                calloc(param_count, sizeof(LLVMTypeRef));

            if (param_types != NULL) {

                LLVMGetParamTypes(
                    function_type,
                    param_types
                );

                for (unsigned i = 0;
                     i < coerce_count;
                     i++) {

                    if (arguments[i] == NULL)
                        continue;

                    LLVMTypeRef arg_type =
                        LLVMTypeOf(arguments[i]);
                    LLVMTypeRef param_type =
                        param_types[i];

                    if (LLVMGetTypeKind(arg_type) !=
                            LLVMIntegerTypeKind ||
                        LLVMGetTypeKind(param_type) !=
                            LLVMIntegerTypeKind)
                        continue;

                    unsigned arg_width =
                        LLVMGetIntTypeWidth(arg_type);
                    unsigned param_width =
                        LLVMGetIntTypeWidth(param_type);

                    if (arg_width > param_width) {

                        arguments[i] =
                            LLVMBuildTrunc(
                                compiler->builder,
                                arguments[i],
                                param_type,
                                "rivo.ffi.trunc"
                            );
                    }
                    else if (arg_width < param_width) {

                        arguments[i] =
                            LLVMBuildSExt(
                                compiler->builder,
                                arguments[i],
                                param_type,
                                "rivo.ffi.sext"
                            );
                    }
                }

                free(param_types);
            }
        }
    }

    /*
     * Una llamada que no devuelve nada no puede llevar
     * nombre en su resultado: LLVM recibe "" en lugar de
     * NULL porque Twine no tolera punteros nulos.
     */
    const char *result_name = "call";

    if (LLVMGetTypeKind(
            LLVMGetReturnType(function_type)
        ) == LLVMVoidTypeKind) {

        result_name = "";
    }

    LLVMValueRef result =
        LLVMBuildCall2(
            compiler->builder,
            function_type,
            function,
            arguments,
            argument_count,
            result_name
        );

    free(arguments);

    /*
     * Los builtins devuelven bool como i8 (uint8_t en C);
     * el bool de E++ se representa como i1.
     */
    if (node->expr_type != NULL &&
        node->expr_type->kind == TYPE_BOOL) {

        LLVMTypeRef result_type =
            LLVMTypeOf(result);

        if (LLVMGetTypeKind(result_type) ==
                LLVMIntegerTypeKind &&
            LLVMGetIntTypeWidth(result_type) == 8) {

            result = LLVMBuildTrunc(
                compiler->builder,
                result,
                llvm_boolean_type(compiler),
                "rivo.bool"
            );
        }
    }

    /*
     * Retorno FFI de ancho fijo promovido a `int` (i64)
     * por la semántica: extender el valor estrecho con
     * signo o ceros según corresponda.
     */
    if (node->expr_type != NULL &&
        node->expr_type->kind == TYPE_INT) {

        LLVMTypeRef result_type =
            LLVMTypeOf(result);

        if (LLVMGetTypeKind(result_type) ==
                LLVMIntegerTypeKind) {

            unsigned width =
                LLVMGetIntTypeWidth(result_type);

            if (width < 64) {

                if (node->data.call.ffi_return_unsigned) {

                    result = LLVMBuildZExt(
                        compiler->builder,
                        result,
                        llvm_int_type(compiler),
                        "rivo.int"
                    );

                } else {

                    result = LLVMBuildSExt(
                        compiler->builder,
                        result,
                        llvm_int_type(compiler),
                        "rivo.int"
                    );
                }
            }
        }
    }

    /*
     * `-> string owned`: copiar el resultado C a la
     * arena y liberar el original con free.
     */
    if (node->data.call.ffi_string_owned) {

        LLVMTypeRef pointer_type =
            llvm_opaque_pointer(compiler);

        LLVMTypeRef copy_params[] = { pointer_type };

        LLVMValueRef copy_fn =
            llvm_add_runtime_function(
                compiler,
                "rivo_string_copy_arena",
                pointer_type,
                copy_params,
                1
            );

        LLVMTypeRef free_params[] = { pointer_type };

        LLVMValueRef free_fn =
            llvm_add_runtime_function(
                compiler,
                "rivo_free_text",
                LLVMVoidTypeInContext(compiler->context),
                free_params,
                1
            );

        if (copy_fn != NULL && free_fn != NULL) {

            LLVMValueRef copied =
                LLVMBuildCall2(
                    compiler->builder,
                    LLVMGlobalGetValueType(copy_fn),
                    copy_fn,
                    &result,
                    1,
                    "rivo.owned"
                );

            LLVMBuildCall2(
                compiler->builder,
                LLVMGlobalGetValueType(free_fn),
                free_fn,
                &result,
                1,
                ""
            );

            result = copied;
        }
    }

    return result;
}


/* =========================================================
 * INDEX
 * ========================================================= */

LLVMValueRef llvm_generate_index(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    /*
     * El tipo estático del objeto (anotado por la semántica
     * en expr_type) distingue list de map: ambos se
     * representan como un puntero opaco.
     */
    Type *object_type =
        (node->data.index.object != NULL)
            ? node->data.index.object->expr_type
            : NULL;

    if (object_type == NULL ||
        (object_type->kind != TYPE_LIST &&
         object_type->kind != TYPE_MAP)) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "el tipo no se puede indexar"
        );

        return NULL;
    }

    LLVMValueRef object =
        llvm_generate_expression(
            compiler,
            node->data.index.object
        );

    LLVMValueRef key =
        llvm_generate_expression(
            compiler,
            node->data.index.index
        );

    if (object == NULL ||
        key == NULL)
        return NULL;

    LLVMValueRef key_bits =
        llvm_value_to_i64(
            compiler,
            key,
            node->line,
            node->column
        );

    if (key_bits == NULL)
        return NULL;

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMValueRef arguments[3];

    arguments[0] = object;
    arguments[1] = key_bits;
    arguments[2] =
        llvm_generate_int(compiler, node->line);

    LLVMTypeRef parameters[] = {
        pointer_type,
        int_type,
        int_type
    };

    const char *runtime_name = NULL;
    const Type *result_type = NULL;

    if (object_type->kind == TYPE_LIST) {

        runtime_name = "rivo_list_get";
        result_type = object_type->element_type;

    } else {

        runtime_name = "rivo_map_get";
        result_type = object_type->value_type;
    }

    LLVMValueRef raw =
        llvm_call_runtime(
            compiler,
            runtime_name,
            int_type,
            parameters,
            3,
            arguments
        );

    if (raw == NULL)
        return NULL;

    return llvm_value_from_i64(
        compiler,
        raw,
        result_type,
        node->line,
        node->column
    );
}


/* =========================================================
 * MEMBER
 * ========================================================= */

LLVMValueRef llvm_generate_member(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    /*
     * Un miembro como valor (numeros.len sin los paréntesis)
     * no tiene representación: E++ no tiene funciones de
     * primer orden. Las llamadas de método se resuelven en
     * llvm_generate_call().
     */

    if (compiler == NULL ||
        node == NULL)
        return NULL;

    ASTNode *object_node =
        node->data.member.object;

    if (object_node != NULL &&
        object_node->expr_type != NULL &&
        object_node->expr_type->kind == TYPE_STRUCT) {
        LLVMValueRef address =
            llvm_generate_member_address(
                compiler,
                node
            );

        if (address == NULL)
            return NULL;

        return LLVMBuildLoad2(
            compiler->builder,
            llvm_type(compiler, node->expr_type),
            address,
            "struct.field"
        );
    }

    llvm_report_error(
        compiler,
        ERROR_CODEGEN_INVALID_OPERATION,
        node->line,
        node->column,
        "el miembro de una lista se debe invocar con ()"
    );

    return NULL;
}


/* =========================================================
 * LISTAS
 * ========================================================= */

LLVMValueRef llvm_generate_list(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    /*
     * La lista se construye en el runtime:
     *
     *     lista = rivo_list_new(tag)
     *     rivo_list_add(lista, e1)
     *     rivo_list_add(lista, e2)
     *
     * El tipo del elemento lo aporta la semántica
     * (expr_type del literal), así que funcionan también
     * las listas cuyos elementos son expresiones: [a, b + 1].
     *
     * Elementos lista/map/null usan tag 0: en el runtime se
     * comparan por identidad, que es el comportamiento
     * correcto para valores referenciales.
     */

    Type *list_info = node->expr_type;

    if (list_info == NULL ||
        list_info->kind != TYPE_LIST ||
        list_info->element_type == NULL) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "lista sin tipo de elemento"
        );

        return NULL;
    }

    int tag =
        llvm_type_tag(list_info->element_type);

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMTypeRef void_type =
        LLVMVoidTypeInContext(compiler->context);

    LLVMTypeRef new_parameters[] = {
        int_type
    };

    LLVMValueRef new_arguments[] = {
        llvm_generate_int(compiler, tag)
    };

    LLVMValueRef list =
        llvm_call_runtime(
            compiler,
            "rivo_list_new",
            pointer_type,
            new_parameters,
            1,
            new_arguments
        );

    if (list == NULL)
        return NULL;

    LLVMTypeRef add_parameters[] = {
        pointer_type,
        int_type
    };

    int count =
        node->data.list_literal.elements.count;

    for (int i = 0; i < count; i++) {

        LLVMValueRef value =
            llvm_generate_expression(
                compiler,
                node->data.list_literal.elements.items[i]
            );

        if (value == NULL)
            return NULL;

        LLVMValueRef bits =
            llvm_value_to_i64(
                compiler,
                value,
                node->line,
                node->column
            );

        if (bits == NULL)
            return NULL;

        LLVMValueRef add_arguments[] = {
            list,
            bits
        };

        if (llvm_call_runtime(
                compiler,
                "rivo_list_add",
                void_type,
                add_parameters,
                2,
                add_arguments
            ) == NULL) {

            return NULL;
        }
    }

    return list;
}


/* =========================================================
 * MAP
 * ========================================================= */

LLVMValueRef llvm_generate_map(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    /*
     * El map se construye en el runtime:
     *
     *     map = rivo_map_new(ktag, vtag)
     *     rivo_map_set(map, clave, valor)   (por entrada)
     *
     * Los tags vienen del tipo anotado por la semántica
     * (expr_type del literal) y deciden cómo compara el
     * runtime las claves.
     */

    Type *map_info = node->expr_type;

    if (map_info == NULL ||
        map_info->kind != TYPE_MAP) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "map sin tipo de clave o valor"
        );

        return NULL;
    }

    int key_tag =
        llvm_type_tag(map_info->key_type);

    int value_tag =
        llvm_type_tag(map_info->value_type);

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMTypeRef void_type =
        LLVMVoidTypeInContext(compiler->context);

    LLVMTypeRef new_parameters[] = {
        int_type,
        int_type
    };

    LLVMValueRef new_arguments[] = {
        llvm_generate_int(compiler, key_tag),
        llvm_generate_int(compiler, value_tag)
    };

    LLVMValueRef map =
        llvm_call_runtime(
            compiler,
            "rivo_map_new",
            pointer_type,
            new_parameters,
            2,
            new_arguments
        );

    if (map == NULL)
        return NULL;

    LLVMTypeRef set_parameters[] = {
        pointer_type,
        int_type,
        int_type
    };

    int count =
        node->data.map_literal.entries.count;

    for (int i = 0; i < count; i++) {

        ASTMapEntry *entry =
            node->data.map_literal.entries.items[i];

        LLVMValueRef key =
            llvm_generate_expression(
                compiler,
                entry->key
            );

        LLVMValueRef value =
            llvm_generate_expression(
                compiler,
                entry->value
            );

        if (key == NULL ||
            value == NULL)
            return NULL;

        LLVMValueRef key_bits =
            llvm_value_to_i64(
                compiler,
                key,
                node->line,
                node->column
            );

        LLVMValueRef value_bits =
            llvm_value_to_i64(
                compiler,
                value,
                node->line,
                node->column
            );

        if (key_bits == NULL ||
            value_bits == NULL)
            return NULL;

        LLVMValueRef set_arguments[] = {
            map,
            key_bits,
            value_bits
        };

        if (llvm_call_runtime(
                compiler,
                "rivo_map_set",
                void_type,
                set_parameters,
                3,
                set_arguments
            ) == NULL) {

            return NULL;
        }
    }

    return map;
}


/* =========================================================
 * RETURN
 * ========================================================= */

/*
 * `return;` sin valor y final implícito de una función.
 *
 * `fn main():` se genera como i32, así que aquí se devuelve
 * 0 y el proceso sale con código de salida 0. Las demás
 * funciones void terminan con ret void.
 */
static LLVMValueRef llvm_build_empty_return(
    LLVMCompiler *compiler
)
{
    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMTypeRef function_type =
        LLVMGlobalGetValueType(function);

    LLVMTypeRef return_type =
        LLVMGetReturnType(function_type);

    if (LLVMGetTypeKind(return_type) ==
            LLVMIntegerTypeKind &&
        LLVMGetIntTypeWidth(return_type) == 32) {

        return LLVMBuildRet(
            compiler->builder,
            LLVMConstInt(return_type, 0, 0)
        );
    }

    return LLVMBuildRetVoid(
        compiler->builder
    );
}


LLVMValueRef llvm_generate_return(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (node->data.return_stmt.value == NULL) {

        return llvm_build_empty_return(compiler);
    }

    LLVMValueRef value =
        llvm_generate_expression(
            compiler,
            node->data.return_stmt.value
        );

    if (value == NULL)
        return NULL;

    return LLVMBuildRet(
        compiler->builder,
        value
    );
}


/* =========================================================
 * IF
 * ========================================================= */

LLVMValueRef llvm_generate_if(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    LLVMValueRef condition =
        llvm_generate_expression(
            compiler,
            node->data.if_stmt.condition
        );

    if (condition == NULL)
        return NULL;

    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef then_block =
        llvm_create_block(
            compiler,
            function,
            "if.then"
        );

    LLVMBasicBlockRef else_block =
        llvm_create_block(
            compiler,
            function,
            "if.else"
        );

    LLVMBasicBlockRef end_block =
        llvm_create_block(
            compiler,
            function,
            "if.end"
        );

    LLVMBuildCondBr(
        compiler->builder,
        condition,
        then_block,
        else_block
    );

    /*
     * THEN
     */
    LLVMPositionBuilderAtEnd(
        compiler->builder,
        then_block
    );

    llvm_generate_statement(
        compiler,
        node->data.if_stmt.then_branch
    );

    /*
     * El ultimo bloque generado puede no ser then_block:
     * si el cuerpo termino en un bloque de anidacion
     * (otro if/while) hay que cerrar ESE bloque.
     */
    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(
                compiler->builder
            )
        ) == NULL) {

        LLVMBuildBr(
            compiler->builder,
            end_block
        );
    }

    /*
     * ELSE
     */
    LLVMPositionBuilderAtEnd(
        compiler->builder,
        else_block
    );

    if (node->data.if_stmt.else_branch != NULL) {

        llvm_generate_statement(
            compiler,
            node->data.if_stmt.else_branch
        );
    }

    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(
                compiler->builder
            )
        ) == NULL) {

        LLVMBuildBr(
            compiler->builder,
            end_block
        );
    }

    /*
     * END
     */
    LLVMPositionBuilderAtEnd(
        compiler->builder,
        end_block
    );

    return NULL;
}


/* =========================================================
 * WHILE
 * ========================================================= */

LLVMValueRef llvm_generate_while(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef condition_block =
        llvm_create_block(
            compiler,
            function,
            "while.condition"
        );

    LLVMBasicBlockRef body_block =
        llvm_create_block(
            compiler,
            function,
            "while.body"
        );

    LLVMBasicBlockRef end_block =
        llvm_create_block(
            compiler,
            function,
            "while.end"
        );

    LLVMBuildBr(
        compiler->builder,
        condition_block
    );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        condition_block
    );

    LLVMValueRef condition =
        llvm_generate_expression(
            compiler,
            node->data.while_stmt.condition
        );

    if (condition == NULL)
        return NULL;

    LLVMBuildCondBr(
        compiler->builder,
        condition,
        body_block,
        end_block
    );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        body_block
    );

    /*
     * Registrar el contexto del bucle para break/continue.
     */
    if (compiler->loop_depth < 64) {

        compiler->loop_break_stack[
            compiler->loop_depth
        ] = end_block;

        compiler->loop_continue_stack[
            compiler->loop_depth
        ] = condition_block;

        compiler->loop_depth++;
    }

    llvm_generate_statement(
        compiler,
        node->data.while_stmt.body
    );

    if (compiler->loop_depth > 0)
        compiler->loop_depth--;

    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(
                compiler->builder
            )
        ) == NULL) {

        LLVMBuildBr(
            compiler->builder,
            condition_block
        );
    }

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        end_block
    );

    return NULL;
}


/* =========================================================
 * FOR
 * ========================================================= */

LLVMValueRef llvm_generate_for(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    Type *element_type =
        node->data.for_stmt.element_type;

    if (element_type == NULL) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "for sin tipo de elemento"
        );

        return NULL;
    }

    /*
     * El iterable es una referencia opaca a una lista o a
     * un map. La longitud se pregunta al runtime en cada
     * vuelta, así que un add() dentro del cuerpo se ve en
     * la siguiente comprobación.
     *
     *     index = 0
     *     mientras index < len(iterable):
     *         variable = iterable[index]     (runtime)
     *         cuerpo
     *         index = index + 1
     *
     * En un map se recorren las claves en orden de
     * inserción (claude.md §10 for variable in iterable).
     */
    Type *iterable_type =
        (node->data.for_stmt.iterable != NULL)
            ? node->data.for_stmt.iterable->expr_type
            : NULL;

    if (iterable_type == NULL ||
        (iterable_type->kind != TYPE_LIST &&
         iterable_type->kind != TYPE_MAP)) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_TYPE,
            node->line,
            node->column,
            "for necesita una list o map"
        );

        return NULL;
    }

    int is_map = iterable_type->kind == TYPE_MAP;

    LLVMValueRef iterable =
        llvm_generate_expression(
            compiler,
            node->data.for_stmt.iterable
        );

    if (iterable == NULL)
        return NULL;

    LLVMTypeRef int_type =
        llvm_int_type(compiler);

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    const char *length_function =
        is_map ? "rivo_map_len" : "rivo_list_len";

    const char *element_function =
        is_map ? "rivo_map_key_at" : "rivo_list_get";

    LLVMTypeRef element_llvm_type =
        llvm_type(
            compiler,
            element_type
        );

    /*
     * El índice es un temporal interno: no se registra.
     * La variable del bucle se registra más abajo, dentro
     * de su propio ámbito.
     */
    LLVMValueRef index_variable =
        LLVMBuildAlloca(
            compiler->builder,
            int_type,
            "for.index"
        );

    LLVMValueRef loop_variable =
        LLVMBuildAlloca(
            compiler->builder,
            element_llvm_type,
            node->data.for_stmt.variable != NULL
                ? node->data.for_stmt.variable
                : "var"
        );

    if (index_variable == NULL ||
        loop_variable == NULL)
        return NULL;

    if (!llvm_store_variable(
            compiler,
            index_variable,
            llvm_generate_int(compiler, 0)
        )) {

        return NULL;
    }

    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef condition_block =
        llvm_create_block(
            compiler,
            function,
            "for.condition"
        );

    LLVMBasicBlockRef body_block =
        llvm_create_block(
            compiler,
            function,
            "for.body"
        );

    LLVMBasicBlockRef step_block =
        llvm_create_block(
            compiler,
            function,
            "for.step"
        );

    LLVMBasicBlockRef end_block =
        llvm_create_block(
            compiler,
            function,
            "for.end"
        );

    LLVMBuildBr(
        compiler->builder,
        condition_block
    );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        condition_block
    );

    LLVMValueRef current_index =
        llvm_load_variable(
            compiler,
            index_variable
        );

    LLVMTypeRef length_parameters[] = {
        pointer_type
    };

    LLVMValueRef length_arguments[] = {
        iterable
    };

    LLVMValueRef length =
        llvm_call_runtime(
            compiler,
            length_function,
            int_type,
            length_parameters,
            1,
            length_arguments
        );

    if (current_index == NULL ||
        length == NULL)
        return NULL;

    LLVMValueRef condition =
        LLVMBuildICmp(
            compiler->builder,
            LLVMIntSLT,
            current_index,
            length,
            "for.test"
        );

    if (condition == NULL)
        return NULL;

    LLVMBuildCondBr(
        compiler->builder,
        condition,
        body_block,
        end_block
    );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        body_block
    );

    /*
     * variable = iterable[index]
     *
     * La lectura va al runtime: en una lista comprueba el
     * rango y en un map devuelve la clave de la posición.
     */
    LLVMValueRef body_index =
        llvm_load_variable(
            compiler,
            index_variable
        );

    if (body_index == NULL)
        return NULL;

    LLVMTypeRef read_parameters[] = {
        pointer_type,
        int_type,
        int_type
    };

    LLVMValueRef read_arguments[] = {
        iterable,
        body_index,
        llvm_generate_int(compiler, node->line)
    };

    LLVMValueRef element_bits =
        llvm_call_runtime(
            compiler,
            element_function,
            int_type,
            read_parameters,
            3,
            read_arguments
        );

    if (element_bits == NULL)
        return NULL;

    LLVMValueRef element =
        llvm_value_from_i64(
            compiler,
            element_bits,
            element_type,
            node->line,
            node->column
        );

    if (element == NULL)
        return NULL;

    if (!llvm_store_variable(
            compiler,
            loop_variable,
            element
        )) {

        return NULL;
    }

    /*
     * Ámbito de la variable del bucle: solo es visible
     * dentro del cuerpo.
     */
    int pushed_scope =
        llvm_push_scope(compiler);

    llvm_scope_declare(
        compiler,
        node->data.for_stmt.variable,
        loop_variable
    );

    /*
     * Registrar el contexto del bucle para break/continue.
     */
    if (compiler->loop_depth < 64) {

        compiler->loop_break_stack[
            compiler->loop_depth
        ] = end_block;

        compiler->loop_continue_stack[
            compiler->loop_depth
        ] = step_block;

        compiler->loop_depth++;
    }

    llvm_generate_statement(
        compiler,
        node->data.for_stmt.body
    );

    if (compiler->loop_depth > 0)
        compiler->loop_depth--;

    if (pushed_scope)
        llvm_pop_scope(compiler);

    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(
                compiler->builder
            )
        ) == NULL) {

        LLVMBuildBr(
            compiler->builder,
            step_block
        );
    }

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        step_block
    );

    LLVMValueRef step_index =
        llvm_load_variable(
            compiler,
            index_variable
        );

    if (step_index == NULL)
        return NULL;

    LLVMValueRef next_index =
        LLVMBuildAdd(
            compiler->builder,
            step_index,
            llvm_generate_int(compiler, 1),
            "for.next"
        );

    if (next_index == NULL)
        return NULL;

    if (!llvm_store_variable(
            compiler,
            index_variable,
            next_index
        )) {

        return NULL;
    }

    LLVMBuildBr(
        compiler->builder,
        condition_block
    );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        end_block
    );

    return NULL;
}


/* =========================================================
 * BREAK / CONTINUE
 * ========================================================= */

LLVMValueRef llvm_generate_break(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    /*
     * El análisis semántico debe rechazar break fuera de
     * bucle; aquí solo comprobamos por seguridad.
     */
    if (compiler->loop_depth <= 0) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "break fuera de un bucle"
        );

        return NULL;
    }

    LLVMBasicBlockRef target =
        compiler->loop_break_stack[
            compiler->loop_depth - 1
        ];

    LLVMBuildBr(
        compiler->builder,
        target
    );

    /*
     * Las sentencias posteriores se generan en un bloque
     * nuevo e inalcanzable para no escribir tras el
     * terminador; el bloque de merge posterior se encarga
     * de cerrarlo si hace falta.
     */
    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef after =
        llvm_create_block(
            compiler,
            function,
            "break.after"
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        after
    );

    return NULL;
}


LLVMValueRef llvm_generate_continue(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    if (compiler->loop_depth <= 0) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "continue fuera de un bucle"
        );

        return NULL;
    }

    LLVMBasicBlockRef target =
        compiler->loop_continue_stack[
            compiler->loop_depth - 1
        ];

    LLVMBuildBr(
        compiler->builder,
        target
    );

    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef after =
        llvm_create_block(
            compiler,
            function,
            "continue.after"
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        after
    );

    return NULL;
}


/* =========================================================
 * TRY
 *
 * try/catch con setjmp/longjmp (decisión de diseño
 * 2026-10-08, usuario; claude.md §15):
 *
 *     try:
 *         ...
 *     catch error:
 *         ...
 *
 * El codegen genera el setjmp en el marco de la función
 * (lo exige la C estándar: un setjmp en un marco ya
 * devuelto es indefinido) y empuja el búfer en la pila de
 * manejadores del runtime antes de setjmp. Cualquier
 * rivo_fatal del runtime dentro del cuerpo aplica longjmp
 * aquí y la ejecución entra en el catch con `error` (un
 * string) cargado con el mensaje. Fuera de try, rivo_fatal
 * sigue imprimiendo y saliendo con código 1.
 *
 * return/break/continue dentro de try los rechaza la
 * semántica (try_depth), así que no hay que limpiar la
 * pila de manejadores en esos saltos.
 * ======================================================= */

LLVMValueRef llvm_generate_try(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    /*
     * El parser solo permite un catch, pero el AST admite
     * lista: si hay más, es un error del generador.
     */
    if (node->data.try_stmt.catches.count != 1) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_OPERATION,
            node->line,
            node->column,
            "try necesita exactamente un bloque catch"
        );

        return NULL;
    }

    ASTNode *catch_node =
        node->data.try_stmt.catches.items[0];

    if (catch_node == NULL)
        return NULL;

    LLVMContextRef context =
        compiler->context;

    LLVMTypeRef type_i8 =
        LLVMInt8TypeInContext(context);

    LLVMTypeRef type_i32 =
        LLVMInt32TypeInContext(context);

    LLVMTypeRef type_void =
        LLVMVoidTypeInContext(context);

    /* i8* opaco (LLVM >= 17) */
    LLVMTypeRef type_pointer =
        LLVMPointerType(type_i8, 0);

    LLVMTypeRef type_buffer =
        LLVMArrayType(
            type_i8,
            RIVO_TRY_BUFFER_SIZE
        );

    LLVMValueRef function =
        LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(
                compiler->builder
            )
        );

    LLVMBasicBlockRef resume_block =
        LLVMGetInsertBlock(
            compiler->builder
        );

    /*
     * El jmp_buf y la variable de error se alojan SIEMPRE
     * en el bloque de entrada: el try puede estar dentro
     * de un bucle y reasignar el alloca en cada vuelta
     * machacaría el manejador activo (la C estándar exige
     * que el búfer del setjmp siga vivo al saltar).
     */
    LLVMBasicBlockRef entry_block =
        LLVMGetFirstBasicBlock(function);

    LLVMValueRef entry_terminator =
        LLVMGetBasicBlockTerminator(entry_block);

    if (entry_terminator != NULL)
        LLVMPositionBuilderBefore(
            compiler->builder,
            entry_terminator
        );
    else
        LLVMPositionBuilderAtEnd(
            compiler->builder,
            entry_block
        );

    LLVMValueRef buffer =
        LLVMBuildAlloca(
            compiler->builder,
            type_buffer,
            "try.buf"
        );

    LLVMSetAlignment(buffer, 16);

    /*
     * Ámbito del catch: la variable de error se declara
     * dentro, pero SOLO cuando se genera el cuerpo del
     * catch, para que el cuerpo del try siga resolviendo
     * los sombreados hacia fuera.
     */
    int scope_pushed =
        llvm_push_scope(compiler);

    LLVMValueRef error_variable =
        LLVMBuildAlloca(
            compiler->builder,
            type_pointer,
            catch_node->data.catch_stmt.error_name
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        resume_block
    );

    LLVMBasicBlockRef body_block =
        llvm_create_block(
            compiler,
            function,
            "try.body"
        );

    LLVMBasicBlockRef catch_block =
        llvm_create_block(
            compiler,
            function,
            "try.catch"
        );

    LLVMBasicBlockRef end_block =
        llvm_create_block(
            compiler,
            function,
            "try.end"
        );

    /*
     * Runtime: rivo_try_push (pila de manejadores),
     * setjmp (del C), rivo_try_pop y rivo_try_error.
     */
    LLVMTypeRef type_pointer_params[1] = {
        type_pointer
    };

    LLVMTypeRef push_type =
        LLVMFunctionType(
            type_void,
            type_pointer_params,
            1,
            0
        );

    LLVMValueRef push_function =
        llvm_add_runtime_function(
            compiler,
            "rivo_try_push",
            type_void,
            type_pointer_params,
            1
        );

    LLVMTypeRef pop_type =
        LLVMFunctionType(
            type_void,
            NULL,
            0,
            0
        );

    LLVMValueRef pop_function =
        llvm_add_runtime_function(
            compiler,
            "rivo_try_pop",
            type_void,
            NULL,
            0
        );

    LLVMTypeRef error_type =
        LLVMFunctionType(
            type_pointer,
            NULL,
            0,
            0
        );

    LLVMValueRef error_function =
        llvm_add_runtime_function(
            compiler,
            "rivo_try_error",
            type_pointer,
            NULL,
            0
        );

    LLVMTypeRef setjmp_type =
        LLVMFunctionType(
            type_i32,
            type_pointer_params,
            1,
            0
        );

    LLVMValueRef setjmp_function =
        llvm_add_runtime_function(
            compiler,
            "setjmp",
            type_i32,
            type_pointer_params,
            1
        );

    /* rivo_try_push(&buffer) antes de setjmp. */
    LLVMValueRef push_arguments[1] = {
        buffer
    };

    LLVMBuildCall2(
        compiler->builder,
        push_type,
        push_function,
        push_arguments,
        1,
        ""
    );

    /*
     * setjmp devuelve 0 en la llamada original y distinto
     * de cero tras el longjmp del runtime.
     */
    LLVMValueRef setjmp_result =
        LLVMBuildCall2(
            compiler->builder,
            setjmp_type,
            setjmp_function,
            push_arguments,
            1,
            "try.setjmp"
        );

    LLVMValueRef is_normal =
        LLVMBuildICmp(
            compiler->builder,
            LLVMIntEQ,
            setjmp_result,
            LLVMConstInt(type_i32, 0, 0),
            "try.ok"
        );

    LLVMBuildCondBr(
        compiler->builder,
        is_normal,
        body_block,
        catch_block
    );

    /* Cuerpo del try: sin manejador propio al salir. */
    LLVMPositionBuilderAtEnd(
        compiler->builder,
        body_block
    );

    llvm_generate_statement(
        compiler,
        node->data.try_stmt.try_body
    );

    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(compiler->builder)
        ) == NULL) {

        LLVMBuildCall2(
            compiler->builder,
            pop_type,
            pop_function,
            NULL,
            0,
            ""
        );

        LLVMBuildBr(
            compiler->builder,
            end_block
        );
    }

    /*
     * Catch: se desapila el manejador de este try (el
     * longjmp ya salió de aquí) y se carga el mensaje en
     * la variable de error.
     */
    LLVMPositionBuilderAtEnd(
        compiler->builder,
        catch_block
    );

    LLVMBuildCall2(
        compiler->builder,
        pop_type,
        pop_function,
        NULL,
        0,
        ""
    );

    LLVMValueRef message =
        LLVMBuildCall2(
            compiler->builder,
            error_type,
            error_function,
            NULL,
            0,
            "try.error"
        );

    LLVMBuildStore(
        compiler->builder,
        message,
        error_variable
    );

    llvm_scope_declare(
        compiler,
        catch_node->data.catch_stmt.error_name,
        error_variable
    );

    llvm_generate_statement(
        compiler,
        catch_node->data.catch_stmt.body
    );

    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(compiler->builder)
        ) == NULL) {

        LLVMBuildBr(
            compiler->builder,
            end_block
        );
    }

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        end_block
    );

    if (scope_pushed)
        llvm_pop_scope(compiler);

    return NULL;
}


/* =========================================================
 * FUNCIÓN
 * ========================================================= */

static LLVMValueRef llvm_emit_function_body(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef function
)
{
    LLVMBasicBlockRef entry =
        llvm_create_block(
            compiler,
            function,
            "entry"
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        entry
    );

    /*
     * El cuerpo de una funcion async se genera como una
     * funcion normal: el empaquetado del resultado lo hace
     * el trampolin de la fibra.
     */
    int pushed_scope =
        llvm_push_scope(compiler);

    for (int i = 0;
         i < node->data.function_decl.parameters.count;
         i++) {

        ASTParameter *parameter =
            node->data.function_decl.parameters.items[i];

        LLVMValueRef argument =
            LLVMGetParam(
                function,
                i
            );

        LLVMTypeRef type =
            llvm_type(
                compiler,
                parameter->type
            );

        LLVMValueRef variable =
            llvm_create_variable(
                compiler,
                parameter->name,
                type
            );

        if (variable != NULL) {

            llvm_store_variable(
                compiler,
                variable,
                argument
            );
        }
    }

    llvm_generate_statement(
        compiler,
        node->data.function_decl.body
    );

    /*
     * Añadir return implícito para void.
     */
    if (LLVMGetBasicBlockTerminator(
            LLVMGetInsertBlock(
                compiler->builder
            )
        ) == NULL) {

        if (node->data.function_decl.return_type == NULL) {

            llvm_build_empty_return(compiler);

        } else {

            /*
             * El análisis semántico debería garantizar
             * que las funciones con retorno devuelven
             * correctamente.
             */
            llvm_report_error(
                compiler,
                ERROR_CODEGEN_FUNCTION_ERROR,
                node->line,
                node->column,
                "la funcion termina sin devolver un valor"
            );
        }
    }

    if (pushed_scope)
        llvm_pop_scope(compiler);

    return function;
}


static char *llvm_function_suffix(
    const char *name,
    const char *suffix
)
{
    size_t length =
        strlen(name) + strlen(suffix) + 1;

    char *result = malloc(length);

    if (result == NULL)
        return NULL;

    snprintf(result, length, "%s%s", name, suffix);

    return result;
}


/*
 * Desempaqueta un int64 del runtime en el tipo LLVM pedido.
 */
static LLVMValueRef llvm_value_from_bits(
    LLVMCompiler *compiler,
    LLVMValueRef bits,
    LLVMTypeRef type,
    int line,
    int column
)
{
    if (bits == NULL || type == NULL)
        return NULL;

    switch (LLVMGetTypeKind(type)) {

        case LLVMIntegerTypeKind:
        {
            unsigned width =
                LLVMGetIntTypeWidth(type);

            if (width == 64)
                return bits;

            if (width < 64)
                return LLVMBuildTrunc(
                    compiler->builder,
                    bits,
                    type,
                    "rivo.arg"
                );

            break;
        }

        case LLVMDoubleTypeKind:

            return LLVMBuildBitCast(
                compiler->builder,
                bits,
                type,
                "rivo.arg"
            );

        case LLVMPointerTypeKind:

            return LLVMBuildIntToPtr(
                compiler->builder,
                bits,
                type,
                "rivo.arg"
            );

        default:
            break;
    }

    llvm_report_error(
        compiler,
        ERROR_CODEGEN_INVALID_TYPE,
        line,
        column,
        "tipo de parametro no soportado en una funcion async"
    );

    return NULL;
}


/*
 * Trampolin de la fibra: recibe la tarea, despierta los
 * argumentos, llama al cuerpo y completa la tarea con el
 * resultado empaquetado.
 */
static LLVMValueRef llvm_generate_async_trampoline(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef body,
    const char *name
)
{
    LLVMTypeRef trampoline_parameters[1] = {
        llvm_opaque_pointer(compiler)
    };

    LLVMTypeRef trampoline_type =
        LLVMFunctionType(
            LLVMVoidTypeInContext(compiler->context),
            trampoline_parameters,
            1,
            0
        );

    LLVMValueRef trampoline =
        LLVMAddFunction(
            compiler->module,
            name,
            trampoline_type
        );

    LLVMBasicBlockRef entry =
        llvm_create_block(
            compiler,
            trampoline,
            "entry"
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        entry
    );

    LLVMValueRef task =
        LLVMGetParam(trampoline, 0);

    LLVMTypeRef pointer_parameters[1] = {
        llvm_opaque_pointer(compiler)
    };

    LLVMValueRef pointer_arguments[1] = {
        task
    };

    LLVMValueRef args =
        llvm_call_runtime(
            compiler,
            "rivo_task_args",
            llvm_opaque_pointer(compiler),
            pointer_parameters,
            1,
            pointer_arguments
        );

    if (args == NULL)
        return NULL;

    unsigned parameter_count =
        LLVMCountParams(body);

    LLVMValueRef *values =
        parameter_count > 0
            ? calloc(parameter_count, sizeof(LLVMValueRef))
            : NULL;

    if (parameter_count > 0 && values == NULL)
        return NULL;

    for (unsigned i = 0; i < parameter_count; i++) {

        LLVMValueRef index =
            LLVMConstInt(
                llvm_int_type(compiler),
                i,
                0
            );

        LLVMValueRef slot =
            LLVMBuildGEP2(
                compiler->builder,
                llvm_int_type(compiler),
                args,
                &index,
                1,
                "rivo.arg.ptr"
            );

        LLVMValueRef bits =
            LLVMBuildLoad2(
                compiler->builder,
                llvm_int_type(compiler),
                slot,
                "rivo.arg.bits"
            );

        LLVMTypeRef parameter_type =
            LLVMTypeOf(
                LLVMGetParam(body, i)
            );

        values[i] =
            llvm_value_from_bits(
                compiler,
                bits,
                parameter_type,
                node->line,
                node->column
            );

        if (values[i] == NULL) {

            free(values);
            return NULL;
        }
    }

    LLVMTypeRef body_type =
        LLVMGlobalGetValueType(body);

    int body_returns_void =
        LLVMGetTypeKind(LLVMGetReturnType(body_type)) ==
        LLVMVoidTypeKind;

    LLVMValueRef result =
        LLVMBuildCall2(
            compiler->builder,
            body_type,
            body,
            values,
            parameter_count,
            body_returns_void ? "" : "rivo.result"
        );

    free(values);

    LLVMValueRef bits;

    if (node->data.function_decl.return_type == NULL) {

        bits =
            LLVMConstInt(
                llvm_int_type(compiler),
                0,
                0
            );

    } else {

        bits =
            llvm_value_to_i64(
                compiler,
                result,
                node->line,
                node->column
            );

        if (bits == NULL)
            return NULL;
    }

    LLVMTypeRef finish_parameters[2] = {
        llvm_opaque_pointer(compiler),
        llvm_int_type(compiler)
    };

    LLVMValueRef finish_arguments[2] = {
        task,
        bits
    };

    llvm_call_runtime(
        compiler,
        "rivo_task_finish",
        LLVMVoidTypeInContext(compiler->context),
        finish_parameters,
        2,
        finish_arguments
    );

    LLVMBuildRetVoid(compiler->builder);

    return trampoline;
}


/*
 * Spawner: crea la tarea, empaqueta los argumentos y la
 * encola. No ejecuta el cuerpo.
 */
static LLVMValueRef llvm_generate_async_spawner(
    LLVMCompiler *compiler,
    ASTNode *node,
    LLVMValueRef function,
    LLVMValueRef trampoline
)
{
    LLVMBasicBlockRef entry =
        llvm_create_block(
            compiler,
            function,
            "entry"
        );

    LLVMPositionBuilderAtEnd(
        compiler->builder,
        entry
    );

    LLVMValueRef task =
        llvm_call_runtime(
            compiler,
            "rivo_task_new",
            llvm_opaque_pointer(compiler),
            NULL,
            0,
            NULL
        );

    if (task == NULL)
        return NULL;

    LLVMValueRef entry_pointer =
        LLVMBuildPointerCast(
            compiler->builder,
            trampoline,
            llvm_opaque_pointer(compiler),
            "rivo.entry"
        );

    unsigned parameter_count =
        LLVMCountParams(function);

    LLVMTypeRef prepare_parameters[3] = {
        llvm_opaque_pointer(compiler),
        llvm_opaque_pointer(compiler),
        llvm_int_type(compiler)
    };

    LLVMValueRef prepare_arguments[3] = {
        task,
        entry_pointer,
        LLVMConstInt(
            llvm_int_type(compiler),
            parameter_count,
            0
        )
    };

    if (llvm_call_runtime(
            compiler,
            "rivo_task_prepare",
            LLVMVoidTypeInContext(compiler->context),
            prepare_parameters,
            3,
            prepare_arguments
        ) == NULL) {
        return NULL;
    }

    LLVMTypeRef args_parameters[1] = {
        llvm_opaque_pointer(compiler)
    };

    LLVMValueRef args_arguments[1] = {
        task
    };

    LLVMValueRef args =
        llvm_call_runtime(
            compiler,
            "rivo_task_args",
            llvm_opaque_pointer(compiler),
            args_parameters,
            1,
            args_arguments
        );

    if (args == NULL)
        return NULL;

    for (unsigned i = 0; i < parameter_count; i++) {

        LLVMValueRef bits =
            llvm_value_to_i64(
                compiler,
                LLVMGetParam(function, i),
                node->line,
                node->column
            );

        if (bits == NULL)
            return NULL;

        LLVMValueRef index =
            LLVMConstInt(
                llvm_int_type(compiler),
                i,
                0
            );

        LLVMValueRef slot =
            LLVMBuildGEP2(
                compiler->builder,
                llvm_int_type(compiler),
                args,
                &index,
                1,
                "rivo.arg.ptr"
            );

        LLVMBuildStore(
            compiler->builder,
            bits,
            slot
        );
    }

    LLVMTypeRef start_parameters[1] = {
        llvm_opaque_pointer(compiler)
    };

    LLVMValueRef start_arguments[1] = {
        task
    };

    if (llvm_call_runtime(
            compiler,
            "rivo_task_start",
            LLVMVoidTypeInContext(compiler->context),
            start_parameters,
            1,
            start_arguments
        ) == NULL) {
        return NULL;
    }

    LLVMBuildRet(compiler->builder, task);

    return function;
}


static LLVMValueRef llvm_generate_async_function(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    const char *name =
        node->data.function_decl.name;

    char *body_name =
        llvm_function_suffix(name, ".__body");

    char *trampoline_name =
        llvm_function_suffix(name, ".__tramp");

    if (body_name == NULL || trampoline_name == NULL) {

        free(body_name);
        free(trampoline_name);
        return NULL;
    }

    LLVMValueRef body =
        llvm_find_function(compiler, body_name);

    if (body == NULL) {

        body =
            llvm_create_function(
                compiler,
                body_name,
                node->data.function_decl.return_type,
                &node->data.function_decl.parameters,
                node->data.function_decl.is_variadic,
                0
            );
    }

    free(body_name);

    if (body == NULL) {

        free(trampoline_name);
        return NULL;
    }

    if (LLVMCountBasicBlocks(body) == 0) {

        if (llvm_emit_function_body(
                compiler,
                node,
                body
            ) == NULL) {

            free(trampoline_name);
            return NULL;
        }
    }

    LLVMValueRef trampoline =
        llvm_find_function(compiler, trampoline_name);

    if (trampoline == NULL ||
        LLVMCountBasicBlocks(trampoline) == 0) {

        trampoline =
            llvm_generate_async_trampoline(
                compiler,
                node,
                body,
                trampoline_name
            );
    }

    free(trampoline_name);

    if (trampoline == NULL)
        return NULL;

    LLVMValueRef spawner =
        llvm_find_function(compiler, name);

    if (spawner == NULL) {

        spawner =
            llvm_create_function(
                compiler,
                name,
                node->data.function_decl.return_type,
                &node->data.function_decl.parameters,
                node->data.function_decl.is_variadic,
                1
            );
    }

    if (spawner == NULL)
        return NULL;

    if (LLVMCountBasicBlocks(spawner) == 0) {

        if (llvm_generate_async_spawner(
                compiler,
                node,
                spawner,
                trampoline
            ) == NULL) {
            return NULL;
        }
    }

    return spawner;
}


LLVMValueRef llvm_generate_function(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    if (node->data.function_decl.is_async)
        return llvm_generate_async_function(compiler, node);

    LLVMValueRef function =
        llvm_find_function(
            compiler,
            node->data.function_decl.name
        );

    if (function == NULL) {

        function =
            llvm_create_function(
                compiler,
                node->data.function_decl.name,
                node->data.function_decl.return_type,
                &node->data.function_decl.parameters,
                node->data.function_decl.is_variadic,
                0
            );
    }

    if (function == NULL)
        return NULL;

    if (LLVMCountBasicBlocks(function) > 0)
        return function;

    return llvm_emit_function_body(compiler, node, function);
}


/* =========================================================
 * IMPORT
 * ========================================================= */

static ASTNodeList *llvm_import_declarations(
    ASTNode *node
)
{
    if (node == NULL)
        return NULL;

    if (node->kind == AST_IMPORT)
        return &node->data.import_decl.declarations;

    if (node->kind == AST_FROM_IMPORT)
        return &node->data.from_import.declarations;

    return NULL;
}


static int llvm_declare_import_functions(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    ASTNodeList *declarations =
        llvm_import_declarations(node);

    if (compiler == NULL ||
        declarations == NULL)
        return 0;

    for (int i = 0;
         i < declarations->count;
         i++) {

        ASTNode *child =
            declarations->items[i];

        if (child == NULL)
            continue;

        if (child->kind == AST_FUNCTION_DECL) {

            if (llvm_find_function(
                    compiler,
                    child->data.function_decl.name
                ) == NULL) {

                if (llvm_create_function(
                        compiler,
                        child->data.function_decl.name,
                        child->data.function_decl.return_type,
                        &child->data.function_decl.parameters,
                        child->data.function_decl.is_variadic,
                        child->data.function_decl.is_async
                    ) == NULL) {
                    return 0;
                }
            }
        }
        else if (child->kind == AST_IMPORT ||
                 child->kind == AST_FROM_IMPORT)
        {
            if (!llvm_declare_import_functions(
                    compiler,
                    child))
                return 0;
        }
    }

    return 1;
}


int llvm_generate_import(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    ASTNodeList *declarations =
        llvm_import_declarations(node);

    if (compiler == NULL ||
        declarations == NULL)
        return 0;

    int ok = 1;

    for (int i = 0;
         i < declarations->count;
         i++) {

        ASTNode *child =
            declarations->items[i];

        if (child == NULL)
            continue;

        if (child->kind == AST_FUNCTION_DECL) {

            /*
             * La función externa solo se declara: su
             * cuerpo vive en un fichero C.
             */
            if (child->data.function_decl
                    .external_path != NULL)
                continue;

            if (llvm_generate_function(
                    compiler,
                    child
                ) == NULL) {
                ok = 0;
            }
        }
        else if (child->kind == AST_IMPORT ||
                 child->kind == AST_FROM_IMPORT)
        {
            if (!llvm_generate_import(
                    compiler,
                    child)) {
                ok = 0;
            }
        }
        else {
            if (!llvm_generate_declaration(
                    compiler,
                    child)) {
                ok = 0;
            }
        }
    }

    return ok;
}


/* =========================================================
 * DECLARACIONES
 * ========================================================= */

int llvm_generate_declaration(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return 0;

    switch (node->kind) {

        case AST_VAR_DECL:
            return llvm_generate_var(
                compiler,
                node
            );

        case AST_CONST_DECL:
            return llvm_generate_const(
                compiler,
                node
            );

        case AST_FUNCTION_DECL:

            return llvm_generate_function(
                compiler,
                node
            ) != NULL;

        case AST_STRUCT_DECL:
            return 1;

        case AST_IMPORT:
            return llvm_generate_import(
                compiler,
                node
            );

        case AST_FROM_IMPORT:
            return llvm_generate_import(
                compiler,
                node
            );

        case AST_PASS:
            return 1;

        default:
            return 0;
    }
}


/* =========================================================
 * STATEMENTS
 * ========================================================= */

LLVMValueRef llvm_generate_statement(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    switch (node->kind) {

        case AST_VAR_DECL:
            llvm_generate_var(
                compiler,
                node
            );
            return NULL;

        case AST_CONST_DECL:
            llvm_generate_const(
                compiler,
                node
            );
            return NULL;

        case AST_FUNCTION_DECL:
            return llvm_generate_function(
                compiler,
                node
            );

        case AST_BLOCK:

            {
                int pushed =
                    llvm_push_scope(compiler);

                for (int i = 0;
                     i < node->data.block.statements.count;
                     i++) {

                    ASTNode *statement =
                        node->data.block.statements.items[i];

                    llvm_generate_statement(
                        compiler,
                        statement
                    );

                    /*
                     * Si ya existe terminador, no seguir
                     * generando instrucciones en el bloque.
                     */
                    if (LLVMGetInsertBlock(
                            compiler->builder
                        ) != NULL &&
                        LLVMGetBasicBlockTerminator(
                            LLVMGetInsertBlock(
                                compiler->builder
                            )
                        ) != NULL) {

                        break;
                    }
                }

                if (pushed)
                    llvm_pop_scope(compiler);
            }

            return NULL;


        case AST_EXPRESSION_STMT:

            return llvm_generate_expression(
                compiler,
                node->data.expression_stmt.expression
            );


        case AST_RETURN:

            return llvm_generate_return(
                compiler,
                node
            );


        case AST_IF:

            return llvm_generate_if(
                compiler,
                node
            );


        case AST_WHILE:

            return llvm_generate_while(
                compiler,
                node
            );


        case AST_FOR:

            return llvm_generate_for(
                compiler,
                node
            );


        case AST_BREAK:

            return llvm_generate_break(
                compiler,
                node
            );


        case AST_CONTINUE:

            return llvm_generate_continue(
                compiler,
                node
            );


        case AST_PASS:

            return NULL;


        case AST_TRY:

            return llvm_generate_try(
                compiler,
                node
            );


        default:
            return llvm_generate_expression(
                compiler,
                node
            );
    }
}


/* =========================================================
 * EXPRESSIONS
 * ========================================================= */

/*
 * f-string: f"hola {valor}".
 *
 * Cada trozo literal se emite como string global y cada
 * interpolación se convierte a texto con el runtime
 * (rivo_str_from_*). Todo se concatena con
 * rivo_concat_string. El resultado es string (i8*), como
 * ya garantizó el análisis semántico.
 */
static LLVMValueRef llvm_generate_fstring(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    LLVMTypeRef pointer_type =
        llvm_opaque_pointer(compiler);

    LLVMValueRef result = NULL;

    const ASTFStringSegmentList *segments =
        &node->data.fstring.segments;

    for (int i = 0; i < segments->count; i++) {

        ASTFStringSegment *segment =
            segments->items[i];

        if (segment == NULL)
            continue;

        LLVMValueRef part = NULL;

        if (segment->is_expr) {

            part = llvm_generate_expression(
                compiler,
                segment->expr
            );

            if (part == NULL)
                return NULL;

            LLVMTypeRef part_type =
                LLVMTypeOf(part);

            switch (LLVMGetTypeKind(part_type)) {

                /*
                 * string: ya es i8* y se concatena tal cual.
                 */
                case LLVMPointerTypeKind:
                    break;


                case LLVMIntegerTypeKind:
                {
                    unsigned width =
                        LLVMGetIntTypeWidth(part_type);

                    const char *runtime_name = NULL;
                    LLVMTypeRef parameter_type =
                        part_type;

                    if (width == 64) {

                        runtime_name =
                            "rivo_str_from_int";

                    } else if (width == 1) {

                        /*
                         * bool es i1 en E++ y uint8_t en
                         * el runtime (como en print).
                         */
                        runtime_name =
                            "rivo_str_from_bool";

                        parameter_type =
                            LLVMInt8TypeInContext(
                                compiler->context
                            );

                        part = LLVMBuildZExt(
                            compiler->builder,
                            part,
                            parameter_type,
                            "rivo.bool8"
                        );

                    } else if (width == 8) {

                        runtime_name =
                            "rivo_str_from_char";
                    }

                    if (runtime_name == NULL) {

                        llvm_report_error(
                            compiler,
                            ERROR_CODEGEN_INVALID_TYPE,
                            node->line,
                            node->column,
                            "tipo entero no soportado "
                            "en el f-string"
                        );

                        return NULL;
                    }

                    LLVMTypeRef function_type =
                        LLVMFunctionType(
                            pointer_type,
                            &parameter_type,
                            1,
                            0
                        );

                    LLVMValueRef function =
                        llvm_add_runtime_function(
                            compiler,
                            runtime_name,
                            pointer_type,
                            &parameter_type,
                            1
                        );

                    part = LLVMBuildCall2(
                        compiler->builder,
                        function_type,
                        function,
                        &part,
                        1,
                        "rivo.str"
                    );

                    break;
                }


                case LLVMDoubleTypeKind:
                {
                    LLVMTypeRef parameter_type =
                        LLVMDoubleTypeInContext(
                            compiler->context
                        );

                    LLVMTypeRef function_type =
                        LLVMFunctionType(
                            pointer_type,
                            &parameter_type,
                            1,
                            0
                        );

                    LLVMValueRef function =
                        llvm_add_runtime_function(
                            compiler,
                            "rivo_str_from_float",
                            pointer_type,
                            &parameter_type,
                            1
                        );

                    part = LLVMBuildCall2(
                        compiler->builder,
                        function_type,
                        function,
                        &part,
                        1,
                        "rivo.str"
                    );

                    break;
                }


                default:

                    llvm_report_error(
                        compiler,
                        ERROR_CODEGEN_INVALID_TYPE,
                        node->line,
                        node->column,
                        "tipo no soportado en el "
                        "f-string"
                    );

                    return NULL;
            }

        } else {

            part = llvm_generate_string(
                compiler,
                segment->text != NULL
                    ? segment->text
                    : ""
            );
        }

        if (part == NULL)
            return NULL;

        if (result == NULL) {

            result = part;
            continue;
        }

        /*
         * result = rivo_concat_string(result, part)
         */
        LLVMTypeRef parameter_types[2] = {
            pointer_type,
            pointer_type
        };

        LLVMTypeRef function_type =
            LLVMFunctionType(
                pointer_type,
                parameter_types,
                2,
                0
            );

        LLVMValueRef function =
            llvm_add_runtime_function(
                compiler,
                "rivo_concat_string",
                pointer_type,
                parameter_types,
                2
            );

        LLVMValueRef arguments[2] = {
            result,
            part
        };

        result = LLVMBuildCall2(
            compiler->builder,
            function_type,
            function,
            arguments,
            2,
            "rivo.concat"
        );
    }

    /*
     * f"" sin segmentos: cadena vacía.
     */
    if (result == NULL)
        result = llvm_generate_string(compiler, "");

    return result;
}


LLVMValueRef llvm_generate_expression(
    LLVMCompiler *compiler,
    ASTNode *node
)
{
    if (compiler == NULL ||
        node == NULL)
        return NULL;

    switch (node->kind) {

        case AST_IDENTIFIER:

            return llvm_generate_identifier(
                compiler,
                node
            );


        case AST_INT_LITERAL:

            return llvm_generate_int(
                compiler,
                node->data.int_literal.value
            );


        case AST_FLOAT_LITERAL:

            return llvm_generate_float(
                compiler,
                node->data.float_literal.value
            );


        case AST_STRING_LITERAL:

            return llvm_generate_string(
                compiler,
                node->data.string_literal.value
            );


        case AST_CHAR_LITERAL:

            return llvm_generate_char(
                compiler,
                node->data.char_literal.value
            );


        case AST_BOOLEAN_LITERAL:

            return llvm_generate_boolean(
                compiler,
                node->data.boolean_literal.value
            );


        case AST_NULL_LITERAL:

            return llvm_generate_null(
                compiler
            );


        case AST_FSTRING:

            return llvm_generate_fstring(
                compiler,
                node
            );


        case AST_BINARY:

            return llvm_generate_binary(
                compiler,
                node
            );


        case AST_UNARY:

            return llvm_generate_unary(
                compiler,
                node
            );


        case AST_ASSIGN:

            return llvm_generate_assignment(
                compiler,
                node
            );


        case AST_CALL:

            return llvm_generate_call(
                compiler,
                node
            );


        case AST_INDEX:

            return llvm_generate_index(
                compiler,
                node
            );


        case AST_MEMBER:

            return llvm_generate_member(
                compiler,
                node
            );


        case AST_LIST_LITERAL:

            /*
             * `[]` vacío: el parser no puede saber si es
             * lista o map (maps no usan `{}`), así que
             * manda el tipo anotado en la declaración
             * (decisión de diseño 2026-10-06).
             */
            if (node->expr_type != NULL &&
                node->expr_type->kind == TYPE_MAP) {

                return llvm_generate_map(
                    compiler,
                    node
                );
            }

            return llvm_generate_list(
                compiler,
                node
            );


        case AST_MAP_LITERAL:

            return llvm_generate_map(
                compiler,
                node
            );


        case AST_AWAIT:
        {
            LLVMValueRef task =
                llvm_generate_expression(
                    compiler,
                    node->data.await_expr.operand
                );

            if (task == NULL)
                return NULL;

            LLVMTypeRef parameters[1] = {
                llvm_opaque_pointer(compiler)
            };

            LLVMValueRef arguments[1] = {
                task
            };

            LLVMValueRef bits =
                llvm_call_runtime(
                    compiler,
                    "rivo_task_wait",
                    llvm_int_type(compiler),
                    parameters,
                    1,
                    arguments
                );

            if (bits == NULL)
                return NULL;

            if (node->expr_type == NULL)
                return bits;

            return llvm_value_from_i64(
                compiler,
                bits,
                node->expr_type,
                node->line,
                node->column
            );
        }


        default:

            llvm_report_error(
                compiler,
                ERROR_CODEGEN_INVALID_AST,
                node->line,
                node->column,
                "nodo AST no soportado por LLVM"
            );

            return NULL;
    }
}


/* =========================================================
 * GENERACIÓN COMPLETA
 * ========================================================= */

int llvm_generate(
    LLVMCompiler *compiler,
    ASTNode *program
)
{
    if (compiler == NULL ||
        program == NULL)
        return 0;

    if (program->kind != AST_PROGRAM) {

        llvm_report_error(
            compiler,
            ERROR_CODEGEN_INVALID_AST,
            program->line,
            program->column,
            "el nodo raiz no es AST_PROGRAM"
        );

        return 0;
    }

    if (!llvm_prepare_struct_types(
            compiler,
            program
        )) {
        compiler->had_error = 1;
        return 0;
    }

    /*
     * Primera pasada:
     * crear todas las funciones.
     *
     * Esto permite llamadas hacia funciones declaradas
     * posteriormente.
     */
    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {

        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node == NULL)
            continue;

        /*
         * Módulos: declarar las firmas de sus funciones
         * para que las llamadas resuelvan en cualquier
         * orden.
         */
        if (node->kind == AST_IMPORT ||
            node->kind == AST_FROM_IMPORT)
        {
            if (!llvm_declare_import_functions(
                    compiler,
                    node)) {

                compiler->had_error = 1;
                return 0;
            }

            continue;
        }

        if (node->kind != AST_FUNCTION_DECL)
            continue;

        if (llvm_find_function(
                compiler,
                node->data.function_decl.name
            ) == NULL) {

            if (llvm_create_function(
                    compiler,
                    node->data.function_decl.name,
                    node->data.function_decl.return_type,
                    &node->data.function_decl.parameters,
                    node->data.function_decl.is_variadic,
                    node->data.function_decl.is_async
                ) == NULL) {

                compiler->had_error = 1;
                return 0;
            }
        }
    }

    /*
     * Segunda pasada:
     * generar todo el código.
     */
    for (int i = 0;
         i < program->data.program.declarations.count;
         i++) {

        ASTNode *node =
            program->data.program.declarations.items[i];

        if (node == NULL)
            continue;

        /*
         * Las funciones necesitan cuerpo, salvo las
         * externas: ya están declaradas en la primera
         * pasada y su cuerpo vive en un fichero C.
         */
        if (node->kind == AST_FUNCTION_DECL) {

            if (node->data.function_decl
                    .external_path != NULL)
                continue;

            if (llvm_generate_function(
                    compiler,
                    node
                ) == NULL) {

                compiler->had_error = 1;
                continue;
            }

            continue;
        }

        if (!llvm_generate_declaration(
                compiler,
                node
            )) {

            compiler->had_error = 1;
        }
    }

    return !compiler->had_error;
}


/* =========================================================
 * OPTIMIZACIÓN
 * ========================================================= */

int llvm_optimize(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL ||
        compiler->module == NULL)
        return 0;

    if (compiler->config.optimization_level <= 0)
        return 1;

    if (compiler->target_machine == NULL)
        return 0;

    /*
     * Pipeline por defecto de LLVM para el nivel pedido,
     * vía PassBuilder (llvm-c/Transforms/PassBuilder.h,
     * ya incluido por include/llvm.h). -O1 -> default<O1>,
     * -O2 -> default<O2>, -O3 -> default<O3>.
     *
     * Las opciones van explícitas: con NULL,
     * LLVMRunPasses() revienta en LLVM 21.
     *
     * LLVMGetErrorMessage consume el error; la cadena se
     * libera con LLVMDisposeErrorMessage.
     */
    const char *pipeline =
        compiler->config.optimization_level >= 3
            ? "default<O3>"
            : (compiler->config.optimization_level >= 2
                   ? "default<O2>"
                   : "default<O1>");

    LLVMPassBuilderOptionsRef options =
        LLVMCreatePassBuilderOptions();

    LLVMErrorRef error =
        LLVMRunPasses(
            compiler->module,
            pipeline,
            compiler->target_machine,
            options
        );

    LLVMDisposePassBuilderOptions(options);

    if (error != NULL) {

        char *message =
            LLVMGetErrorMessage(error);

        fprintf(
            stderr,
            "SystemError: error de LLVM en %s: %s\n",
            pipeline,
            message != NULL
                ? message
                : "(sin mensaje)"
        );

        if (message != NULL)
            LLVMDisposeErrorMessage(message);

        return 0;
    }

    return 1;
}


/* =========================================================
 * VERIFICACIÓN
 * ========================================================= */

int llvm_verify(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL ||
        compiler->module == NULL)
        return 0;

    char *message = NULL;

    int result =
        LLVMVerifyModule(
            compiler->module,
            LLVMReturnStatusAction,
            &message
        );

    if (result != 0) {

        if (compiler->errors != NULL) {

            error_report_full(
                compiler->errors,
                ERROR_CODEGEN_LLVM_ERROR,
                ERROR_PHASE_CODEGEN,
                0,
                0,
                0,
                "LLVM rechazo el modulo generado",
                message,
                NULL
            );
        }

        compiler->had_error = 1;

        if (message != NULL)
            LLVMDisposeMessage(message);

        return 0;
    }

    if (message != NULL)
        LLVMDisposeMessage(message);

    return 1;
}


/* =========================================================
 * DEBUG / IR
 * ========================================================= */

void llvm_dump(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL ||
        compiler->module == NULL)
        return;

    LLVMDumpModule(
        compiler->module
    );
}


char *llvm_get_ir(
    LLVMCompiler *compiler
)
{
    if (compiler == NULL ||
        compiler->module == NULL)
        return NULL;

    return LLVMPrintModuleToString(
        compiler->module
    );
}


/* =========================================================
 * ESCRITURA IR
 * ========================================================= */

int llvm_write_ir(
    LLVMCompiler *compiler,
    const char *filename
)
{
    if (compiler == NULL ||
        filename == NULL)
        return 0;

    char *message = NULL;

    int result =
        LLVMPrintModuleToFile(
            compiler->module,
            filename,
            &message
        );

    if (result != 0) {

        if (compiler->errors != NULL) {

            error_report_full(
                compiler->errors,
                ERROR_CODEGEN_LLVM_ERROR,
                ERROR_PHASE_CODEGEN,
                0,
                0,
                0,
                "no se pudo escribir el IR",
                message,
                NULL
            );
        }

        compiler->had_error = 1;

        if (message != NULL)
            LLVMDisposeMessage(message);

        return 0;
    }

    if (message != NULL)
        LLVMDisposeMessage(message);

    return 1;
}


/* =========================================================
 * ASSEMBLY
 * ========================================================= */

int llvm_write_assembly(
    LLVMCompiler *compiler,
    const char *filename
)
{
    if (compiler == NULL ||
        compiler->target_machine == NULL ||
        filename == NULL)
        return 0;

    char *message = NULL;

    int result =
        LLVMTargetMachineEmitToFile(
            compiler->target_machine,
            compiler->module,
            (char *)filename,
            LLVMAssemblyFile,
            &message
        );

    if (result != 0) {

        if (compiler->errors != NULL) {

            error_report_full(
                compiler->errors,
                ERROR_CODEGEN_LLVM_ERROR,
                ERROR_PHASE_CODEGEN,
                0,
                0,
                0,
                "no se pudo generar assembly",
                message,
                NULL
            );
        }

        compiler->had_error = 1;

        if (message != NULL)
            LLVMDisposeMessage(message);

        return 0;
    }

    if (message != NULL)
        LLVMDisposeMessage(message);

    return 1;
}


/* =========================================================
 * OBJECT
 * ========================================================= */

int llvm_write_object(
    LLVMCompiler *compiler,
    const char *filename
)
{
    if (compiler == NULL ||
        compiler->target_machine == NULL ||
        filename == NULL)
        return 0;

    char *message = NULL;

    int result =
        LLVMTargetMachineEmitToFile(
            compiler->target_machine,
            compiler->module,
            (char *)filename,
            LLVMObjectFile,
            &message
        );

    if (result != 0) {

        if (compiler->errors != NULL) {

            error_report_full(
                compiler->errors,
                ERROR_CODEGEN_LLVM_ERROR,
                ERROR_PHASE_CODEGEN,
                0,
                0,
                0,
                "no se pudo generar el objeto",
                message,
                NULL
            );
        }

        compiler->had_error = 1;

        if (message != NULL)
            LLVMDisposeMessage(message);

        return 0;
    }

    if (message != NULL)
        LLVMDisposeMessage(message);

    return 1;
}


/* =========================================================
 * EXECUTABLE
 * ========================================================= */

int llvm_write_executable(
    LLVMCompiler *compiler,
    const char *filename
)
{
    /*
     * LLVM no es el linker final.
     *
     * Aquí deberá entrar linker.c:
     *
     * LLVM -> .o -> linker -> executable
     */

    if (compiler == NULL ||
        filename == NULL)
        return 0;

    llvm_report_error(
        compiler,
        ERROR_CODEGEN_TARGET_ERROR,
        0,
        0,
        "la generacion del ejecutable debe realizarse mediante linker.c"
    );

    return 0;
}


/* =========================================================
 * CONFIGURACIÓN DINÁMICA
 * ========================================================= */

void llvm_set_optimization_level(
    LLVMCompiler *compiler,
    int level
)
{
    if (compiler == NULL)
        return;

    if (level < 0)
        level = 0;

    if (level > 3)
        level = 3;

    compiler->config.optimization_level =
        level;
}


void llvm_set_debug_info(
    LLVMCompiler *compiler,
    int enabled
)
{
    if (compiler == NULL)
        return;

    compiler->config.debug_info =
        enabled ? 1 : 0;
}


int llvm_has_error(
    const LLVMCompiler *compiler
)
{
    if (compiler == NULL)
        return 1;

    return compiler->had_error;
}