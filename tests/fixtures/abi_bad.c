/*
 * Fichero con una firma intencionadamente incorrecta:
 * `suma` debería recibir (long long, long long) pero
 * aquí recibe (int, int). La verificación ABI debe
 * detectar el desajuste en compile-time.
 */

int suma(int a, int b)
{
    return a + b;
}
