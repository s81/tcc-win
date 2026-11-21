#include <stdlib.h>
#include <stdint.h>
/* an owner result turned into an integer is no longer tracked */
uintptr_t f(void)
{
    return (uintptr_t)malloc(16);
}
