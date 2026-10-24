#ifndef TYPES_H
#define TYPES_H

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_LIST,
    TYPE_MAP,
    TYPE_TASK,
    TYPE_NULL,
    TYPE_FUNCTION,
    TYPE_STRUCT,

    /* Tipos FFI: anchos fijos de C, solo en firmas externas */
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_SIZE,

    /* Handle opaco (void*): solo en FFI; comparable con null */
    TYPE_OPAQUE,

    /*
     * Puntero a función C (callback). Estructuralmente
     * igual que TYPE_FUNCTION: parameter_types +
     * return_type. Solo como parámetro de firma externa.
     */
    TYPE_CALLBACK,

    /*
     * buffer<T>: puntero + longitud para FFI. Se pasa una
     * list<T> E++ y C recibe dos parámetros ocultos
     * (T*, size). Solo en firmas externas.
     */
    TYPE_BUFFER
} TypeKind;


typedef struct Type Type;

struct Type {

    /* Tipo principal */
    TypeKind kind;

    /* Para list<T> y task<T> */
    Type *element_type;

    /* Para map<K, V> */
    Type *key_type;
    Type *value_type;

    /* Para funciones como valores */
    Type **parameter_types;
    int parameter_count;
    Type *return_type;

    /*
     * Para TYPE_STRUCT: nombre del struct declarado
     * (ej. "Punto"). El campo vive en la definición
     * del struct, no en la anotación de tipo.
     */
    char *struct_name;
};

#endif