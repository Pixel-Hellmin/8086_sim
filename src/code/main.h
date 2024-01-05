#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;

typedef int8_t   i8;
typedef int16_t  i16;

#define global_variable static 
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

struct field {
    u8 length;
    u8 start_bit;
};
struct instruction {
    u8 length;
    u8 opcode;
};

global_variable instruction MovRegMemToFromReg = { 6, 0b00100010 };
global_variable instruction MovImmToRegMem     = { 7, 0b01100011 };
global_variable instruction MovImmToReg        = { 4, 0b00001011 };

global_variable instruction AddRegMemWithRegToEither = { 6, 0b00000000 };
global_variable instruction SubRegMemWithRegToEither = { 6, 0b00001010 };
global_variable instruction CmpRegMemWithRegToEither = { 6, 0b00001110 };

global_variable instruction AddImmToAcc   = { 7, 0b00000010 };
global_variable instruction SubImmFromAcc = { 7, 0b00010110 };
global_variable instruction CmpImmWithAcc = { 7, 0b00011110 };
global_variable instruction AddSubCmpImmXRegMem = { 6, 0b00100000 };

global_variable instruction JNZ    = { 8, 0b01110101 };
global_variable instruction JE     = { 8, 0b01110100 };
global_variable instruction JLE    = { 8, 0b01111110 };
global_variable instruction JB     = { 8, 0b01110010 };
global_variable instruction JBE    = { 8, 0b01110110 };
global_variable instruction JP     = { 8, 0b01111010 };
global_variable instruction JO     = { 8, 0b01110000 };
global_variable instruction JS     = { 8, 0b01111000 };
global_variable instruction JNE    = { 8, 0b01110101 };
global_variable instruction JNL    = { 8, 0b01111101 };
global_variable instruction JG     = { 8, 0b01111111 };
global_variable instruction JNB    = { 8, 0b01110011 };
global_variable instruction JA     = { 8, 0b01110111 };
global_variable instruction JNP    = { 8, 0b01111011 };
global_variable instruction JNO    = { 8, 0b01110001 };
global_variable instruction JNS    = { 8, 0b01111001 };
global_variable instruction LOOP   = { 8, 0b11100010 };
global_variable instruction LOOPZ  = { 8, 0b11100001 };
global_variable instruction LOOPNZ = { 8, 0b11100000 };
global_variable instruction JCXZ   = { 8, 0b11100011 };

global_variable char Registers[][8][3] = {
    {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
    {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"}
};
global_variable char EffectiveAddresses[][9] = {
    "[bx + si", "[bx + di", "[bp + si", "[bp + di", "[si", "[di", "[bp", "[bx"
};
