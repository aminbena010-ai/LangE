/*
 * Fichero de prueba para los tipos FFI de ancho fijo
 * (Fase 1 del plan FFI): i8, i16, i32, u8, u16, u32,
 * u64, size y void explícito.
 *
 * Mapeo E++ <-> C:
 *
 *     i8   -> int8_t        u8   -> uint8_t
 *     i16  -> int16_t       u16  -> uint16_t
 *     i32  -> int32_t       u32  -> uint32_t
 *     u64  -> uint64_t      size -> size_t
 */

#include <stdint.h>
#include <stddef.h>

int8_t doble_i8(int8_t x)
{
    return (int8_t) (x * 2);
}

int16_t doble_i16(int16_t x)
{
    return (int16_t) (x * 2);
}

int32_t doble_i32(int32_t x)
{
    return x * 2;
}

uint8_t doble_u8(uint8_t x)
{
    return (uint8_t) (x * 2);
}

uint16_t doble_u16(uint16_t x)
{
    return (uint16_t) (x * 2);
}

uint32_t doble_u32(uint32_t x)
{
    return x * 2;
}

uint64_t doble_u64(uint64_t x)
{
    return x * 2;
}

size_t triple_size(size_t x)
{
    return x * 3;
}

void sin_retorno(int64_t x)
{
    (void) x;
}

/* Devuelve uint8_t: la semántica lo promueve a int con
   extensión de ceros. */
uint8_t maximo_u8(void)
{
    return 255;
}

/* Devuelve int8_t negativo: la semántica lo promueve a int
   con extensión de signo. */
int8_t minimo_i8(void)
{
    return -128;
}
