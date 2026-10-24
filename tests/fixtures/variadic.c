#include <stdio.h>
#include <stdint.h>

int64_t log_externo(const char *fmt, int64_t a, int64_t b, ...)
{
    return printf(fmt, (int)a, (int)b);
}
