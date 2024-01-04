#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

#define global_variable static 

typedef unsigned char u8;
typedef char          i8;
typedef uint16_t      u16;
typedef int16_t       i16;

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


long read_file(char **buffer, char *file_name, char *mode)
{
    FILE *file;
    if (fopen_s(&file, file_name, mode) != 0) {
        perror("Error opening file");
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    *buffer = (char *)malloc(file_size);
    if (*buffer == nullptr) {
        perror("Memory allocation error");
    }

    size_t bytes_read = fread(*buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        perror("Error reading file");
    }

    fclose(file);

    return file_size;
}

u8 decode_field(field field, u8 byte)
{
    u8 result = byte   << field.start_bit;
    result    = result >> field.start_bit;
    result    = result >> (7 - field.start_bit - field.length + 1);

    return result;
}

u8 decode_instruction(u8 byte, instruction instruction)
{
    return (byte >> 8 - instruction.length) == instruction.opcode;
}

u16 get_u16_at(char *buffer)
{
    u8 disp_lo = buffer[0];
    u8 disp_h  = buffer[1];
    return (u16)disp_h << 8 | (u16)disp_lo;
}

size_t decode_address_or_register(
    char *buffer,
    char *result,
    u8 w,
    u8 mod,
    u8 rm
) {
    size_t offset = 0;
    if (mod == 0b00000011) {
        strcpy(result, Registers[w][rm]);
    } else if (mod == 0b00000000) {
        if (rm == 0b00000110) {
            u16 disp = get_u16_at(&buffer[2]);
            if (disp) {
                strcpy(result, "[");
                sprintf(result + strlen(result), "%d", disp);
                strcat(result, "]");
            }
            offset += 2;
        } else {
            strcpy(result, EffectiveAddresses[rm]);
            strcat(result, "]");
        }
    } else if (mod == 0b00000001) {
            strcpy(result, EffectiveAddresses[rm]);
            u8 disp = buffer[2];
            if (disp) {
                strcat(result, " + ");
                sprintf(result + strlen(result), "%d", disp);
            }
            strcat(result, "]");
            offset++;
    } else if (mod == 0b00000010) {
            strcpy(result, EffectiveAddresses[rm]);
            u16 disp = get_u16_at(&buffer[2]);
            if (disp) {
                strcat(result, " + ");
                sprintf(result + strlen(result), "%d", disp);
            }
            strcat(result, "]");
            offset += 2;
    }

    return offset;
}

int main(int argc, char *argv[])
{
    printf("bits 16\n\n");

    char *buffer = nullptr;
    long file_size = read_file(&buffer, argv[1], (char *)"rb");

    for (size_t byte_index = 0; byte_index < file_size; byte_index += 2)
    {
        u8 first_byte  = buffer[byte_index];
        u8 second_byte = buffer[byte_index + 1];

        if (decode_instruction(first_byte, MovRegMemToFromReg)) {

            field d_field   = { 1, 6 };
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field reg_field = { 3, 2 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 d   = decode_field(d_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 reg = decode_field(reg_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char * dest = Registers[w][reg];
            char src[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     src, w, mod, rm);

            if (!d) {
                printf("mov %s, %s\n", src, dest);
            } else {
                printf("mov %s, %s\n", dest, src);
            }

        } else if (decode_instruction(first_byte, MovImmToRegMem)) {

            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char dest[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     dest, w, mod, rm);
            char src[32];
            if (w) {
                strcpy(src, "word ");
                u16 disp = get_u16_at(&buffer[byte_index + 2]);
                sprintf(src + strlen(src), "%d", disp);
                byte_index += 2;
            } else {
                strcpy(src, "byte ");
                u8 disp = buffer[byte_index + 2];
                sprintf(src + strlen(src), "%d", disp);
                byte_index++;
            }

            printf("mov %s, %s\n", dest, src);

        } else if (decode_instruction(first_byte, MovImmToReg)) {

            field w_field   = { 1, 4 };
            field reg_field = { 3, 5 };

            u8 w   = decode_field(w_field, first_byte);
            u8 reg = decode_field(reg_field, first_byte);

            char * dest = Registers[w][reg];
            char src[32];
            if (w) {
                u16 disp = get_u16_at(&buffer[byte_index + 1]);
                sprintf(src, "%d", disp);
                byte_index++;
            } else {
                u8 disp = buffer[byte_index + 1];
                sprintf(src, "%d", disp);
            }

            printf("mov %s, %s\n", dest, src);

        } else if (decode_instruction(first_byte, AddRegMemWithRegToEither)) {

            field d_field   = { 1, 6 };
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field reg_field = { 3, 2 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 d   = decode_field(d_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 reg = decode_field(reg_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char * dest = Registers[w][reg];
            char src[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     src, w, mod, rm);

            if (!d) {
                printf("add %s, %s\n", src, dest);
            } else {
                printf("add %s, %s\n", dest, src);
            }

        } else if (decode_instruction(first_byte, SubRegMemWithRegToEither)) {

            field d_field   = { 1, 6 };
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field reg_field = { 3, 2 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 d   = decode_field(d_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 reg = decode_field(reg_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char * dest = Registers[w][reg];
            char src[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     src, w, mod, rm);

            if (!d) {
                printf("sub %s, %s\n", src, dest);
            } else {
                printf("sub %s, %s\n", dest, src);
            }

        } else if (decode_instruction(first_byte, CmpRegMemWithRegToEither)) {

            field d_field   = { 1, 6 };
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field reg_field = { 3, 2 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 d   = decode_field(d_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 reg = decode_field(reg_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char * dest = Registers[w][reg];
            char src[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     src, w, mod, rm);

            if (!d) {
                printf("cmp %s, %s\n", src, dest);
            } else {
                printf("cmp %s, %s\n", dest, src);
            }

        } else if (decode_instruction(first_byte, AddImmToAcc)) {

            field w_field = { 1, 7 };
            u8 w = decode_field(w_field, first_byte);

            char dest[3];
            char src[32];
            if (w) { 
                i16 disp = (i16)get_u16_at(&buffer[byte_index + 1]);
                sprintf(src, "%d", disp);
                strcpy(dest, "ax");
                byte_index++;
            } else {
                i8 disp = buffer[byte_index + 1];
                sprintf(src, "%d", disp);
                strcpy(dest, "al");
            }

            printf("add %s, %s\n", dest, src);

        } else if (decode_instruction(first_byte, SubImmFromAcc)) {

            field w_field   = { 1, 7 };
            u8 w   = decode_field(w_field,   first_byte);

            char dest[3];
            char src[32];
            if (w) { 
                i16 disp = (i16)get_u16_at(&buffer[byte_index + 1]);
                sprintf(src, "%d", disp);
                strcpy(dest, "ax");
                byte_index++;
            } else {
                i8 disp = buffer[byte_index + 1];
                sprintf(src, "%d", disp);
                strcpy(dest, "al");
            }

            printf("sub %s, %s\n", dest, src);

        } else if (decode_instruction(first_byte, CmpImmWithAcc)) {

            field w_field = { 1, 7 };
            u8 w = decode_field(w_field, first_byte);

            char dest[3];
            char src[32];
            if (w) { 
                i16 disp = (i16)get_u16_at(&buffer[byte_index + 1]);
                sprintf(src, "%d", disp);
                strcpy(dest, "ax");
                byte_index++;
            } else {
                i8 disp = buffer[byte_index + 1];
                sprintf(src, "%d", disp);
                strcpy(dest, "al");
            }

            printf("cmp %s, %s\n", dest, src);

        } else if (decode_instruction(first_byte, AddSubCmpImmXRegMem)) {

            field s_field   = { 1, 6 };
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field op_field  = { 3, 2 };
            field rm_field  = { 3, 5 };

            u8 s   = decode_field(s_field,   first_byte);
            u8 w   = decode_field(w_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 op  = decode_field(op_field,  second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char dest[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     dest, w, mod, rm);

            char inst[4];
            if (op == 0b00000000) {
                strcpy(inst, "add");
            } else if (op == 0b00000101) {
                strcpy(inst, "sub");
            } else if (op == 0b00000111) {
                strcpy(inst, "cmp");
            }

            char src[32];
            if (w) {
                strcat(inst, " word");
                if (!s) {
                    i16 disp = (i16)get_u16_at(&buffer[byte_index + 2]);
                    sprintf(src, "%d", disp);
                    byte_index += 2;
                } else {
                    i16 disp = (i16)buffer[byte_index + 2];
                    sprintf(src, "%d", disp);
                    byte_index++;
                }
            } else {
                strcat(inst, " byte");
                i8 disp = buffer[byte_index + 2];
                sprintf(src, "%d", disp);
                byte_index++;
            }

            printf("%s %s, %s\n", inst, dest, src);

        } else if (decode_instruction(first_byte, JNZ)) {
            printf("jnz label\n");
        } else if (decode_instruction(first_byte, JE)) {
            printf("je label\n");
        } else if (decode_instruction(first_byte, JLE)) {
            printf("jle label\n");
        } else if (decode_instruction(first_byte, JB)) {
            printf("jb label\n");
        } else if (decode_instruction(first_byte, JBE)) {
            printf("jbe label\n");
        } else if (decode_instruction(first_byte, JP)) {
            printf("jp label\n");
        } else if (decode_instruction(first_byte, JO)) {
            printf("jo label\n");
        } else if (decode_instruction(first_byte, JS)) {
            printf("js label\n");
        } else if (decode_instruction(first_byte, JNE)) {
            printf("jne label\n");
        } else if (decode_instruction(first_byte, JNL)) {
            printf("jnl label\n");
        } else if (decode_instruction(first_byte, JG)) {
            printf("jg label\n");
        } else if (decode_instruction(first_byte, JNB)) {
            printf("jnb label\n");
        } else if (decode_instruction(first_byte, JA)) {
            printf("ja label\n");
        } else if (decode_instruction(first_byte, JNP)) {
            printf("jnp label\n");
        } else if (decode_instruction(first_byte, JNO)) {
            printf("jno label\n");
        } else if (decode_instruction(first_byte, JNS)) {
            printf("jns label\n");
        } else if (decode_instruction(first_byte, LOOP)) {
            printf("loop label\n");
        } else if (decode_instruction(first_byte, LOOPZ)) {
            printf("loopz label\n");
        } else if (decode_instruction(first_byte, LOOPNZ)) {
            printf("loopnz label\n");
        } else if (decode_instruction(first_byte, JCXZ)) {
            printf("jcxz label\n");
        }
    }

    free(buffer);
    
    return 0;
}
