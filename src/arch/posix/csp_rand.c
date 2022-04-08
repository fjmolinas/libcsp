#include <stdlib.h>
#include <csp/arch/csp_rand.h>

uint8_t csp_rand8(void)
{
    return (uint8_t)rand();
}
uint16_t csp_rand16(void)
{
    return rand();
}
