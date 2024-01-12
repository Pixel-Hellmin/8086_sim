#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;

typedef int8_t   s8;
typedef int16_t  s16;

#define global_variable static 
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

struct field
{
    u8 length;
    u8 start_bit;
};

struct inst_code
{
    u8 length;
    u8 opcode;
};

enum instruction_type
{
    mov,
    add,
    sub,
    cmp,
};

struct instruction
{
    instruction_type type;

    char src[32];
    char dest[32];
};

enum RegisterIndex
{
    ax,
    cx,
    dx,
    bx,
    sp,
    bp,
    si,
    di,

    count
};

global_variable inst_code MovRegMemToFromReg = { 6, 0b00100010 };
global_variable inst_code MovImmToRegMem     = { 7, 0b01100011 };
global_variable inst_code MovImmToReg        = { 4, 0b00001011 };

global_variable inst_code AddRegMemWithRegToEither = { 6, 0b00000000 };
global_variable inst_code SubRegMemWithRegToEither = { 6, 0b00001010 };
global_variable inst_code CmpRegMemWithRegToEither = { 6, 0b00001110 };

global_variable inst_code AddImmToAcc   = { 7, 0b00000010 };
global_variable inst_code SubImmFromAcc = { 7, 0b00010110 };
global_variable inst_code CmpImmWithAcc = { 7, 0b00011110 };
global_variable inst_code AddSubCmpImmXRegMem = { 6, 0b00100000 };

global_variable inst_code JNZ    = { 8, 0b01110101 };
global_variable inst_code JE     = { 8, 0b01110100 };
global_variable inst_code JLE    = { 8, 0b01111110 };
global_variable inst_code JB     = { 8, 0b01110010 };
global_variable inst_code JBE    = { 8, 0b01110110 };
global_variable inst_code JP     = { 8, 0b01111010 };
global_variable inst_code JO     = { 8, 0b01110000 };
global_variable inst_code JS     = { 8, 0b01111000 };
global_variable inst_code JNE    = { 8, 0b01110101 };
global_variable inst_code JNL    = { 8, 0b01111101 };
global_variable inst_code JG     = { 8, 0b01111111 };
global_variable inst_code JNB    = { 8, 0b01110011 };
global_variable inst_code JA     = { 8, 0b01110111 };
global_variable inst_code JNP    = { 8, 0b01111011 };
global_variable inst_code JNO    = { 8, 0b01110001 };
global_variable inst_code JNS    = { 8, 0b01111001 };
global_variable inst_code LOOP   = { 8, 0b11100010 };
global_variable inst_code LOOPZ  = { 8, 0b11100001 };
global_variable inst_code LOOPNZ = { 8, 0b11100000 };
global_variable inst_code JCXZ   = { 8, 0b11100011 };

global_variable char RegisterNames[][8][3] =
{
    {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
    {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}
};
global_variable char EffectiveAddresses[][9] =
{
    "[bx + si", "[bx + di", "[bp + si", "[bp + di", "[si", "[di", "[bp", "[bx"
};
