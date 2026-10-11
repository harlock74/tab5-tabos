// Verify the narrow $0080 helper against its binary 6502 flag contract.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../src/basic_kernal.c"

unsigned char RAM[65536];
unsigned char A, X, Y, S, V, B, D, I, C, N, Z;
unsigned short PC;

void basic_runtime_checkpoint(void)
{
}
void basic_storage_init(void)
{
}
void basic_storage_device(void)
{
    abort();
}
void basic_storage_name(void)
{
    abort();
}
const char* basic_storage_load(void)
{
    abort();
}
const char* basic_storage_save(void)
{
    abort();
}
int basic_runtime_read_byte(bool command)
{
    (void) command;
    abort();
}
uint8_t basic_runtime_get_byte(void)
{
    abort();
}
void basic_runtime_put_byte(uint8_t byte)
{
    (void) byte;
    abort();
}
void basic_runtime_write(const char* text)
{
    (void) text;
    abort();
}
bool basic_runtime_break(void)
{
    abort();
}
uint8_t basic_runtime_column(void)
{
    abort();
}
uint32_t basic_runtime_jiffies(void)
{
    abort();
}
void basic_runtime_set_jiffies(uint32_t value)
{
    (void) value;
    abort();
}
void basic_unsupported(void)
{
    abort();
}
_Noreturn void basic_engine_exit(int status)
{
    (void) status;
    abort();
}

int main(void)
{
    for (unsigned value = 0U; value <= 255U; ++value) {
        if (value == ' ') {
            continue;
        }
        memset(RAM, 0, sizeof(RAM));
        A = (uint8_t) value;
        X = 42U;
        Y = 7U;
        S = 0xe0U;
        B = I = 1U;
        D     = 0U; // BASIC's binary arithmetic mode; no decimal-mode execution API.
        V = C = N = Z = 1U;
        RAM[0x7a]     = 0x34U;
        RAM[0x7b]     = 0x12U;
        PC            = 0x0080U;
        assert(kernal_dispatch() == 1U);
        assert(A == value && X == 42U && Y == 7U && S == 0xe0U);
        assert(C == (value < 0x30U) && Z == (value == 0U) && N == (value >= 0x80U));
        assert(V == (value >= 0x80U && value < 0xb0U));
        assert(B == 1U && I == 1U && D == 0U);
        assert(RAM[0x7a] == 0x34U && RAM[0x7b] == 0x12U);
    }
    A           = ' ';
    RAM[0x7a]   = 0xffU;
    RAM[0x7b]   = 0x09U;
    RAM[0x0a00] = ' ';
    RAM[0x0a01] = '9';
    assert(kernal_dispatch() == 1U);
    assert(A == '9' && C == 0U && N == 0U && Z == 0U && V == 0U);
    assert(RAM[0x7a] == 1U && RAM[0x7b] == 0x0aU);
    A         = ' ';
    V         = 1U;
    RAM[0x7a] = RAM[0x7b] = 0xffU;
    RAM[0]                = ':';
    assert(kernal_dispatch() == 1U);
    assert(A == ':' && C == 1U && Z == 1U && V == 1U);
    assert(RAM[0x7a] == 0U && RAM[0x7b] == 0U);
    assert(X == 42U && Y == 7U && S == 0xe0U);
    return 0;
}
