#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

#define global_variable static 

typedef unsigned char u8;
typedef uint16_t      u16;

struct field {
    u8 length;
    u8 start_bit;
};
struct instruction {
    u8 length;
    u8 opcode;
};

global_variable instruction RegMemToFromReg = { 6, 0b00100010 };
global_variable instruction ImmToReg        = { 4, 0b00001011 };

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

void debug_print_field(field field, u8 byte)
{
    for (int bit = 0; bit < field.length; ++bit)
    {
        putchar((byte & (1 << (7 - field.start_bit - bit))) ? '1' : '0');
    }
    putchar(' ');
}

void debug_print_byte(u8 byte)
{

    for (int bit = 7; bit >= 0; --bit)
    {
        putchar((byte & (1 << bit)) ? '1' : '0');
    }
    putchar(' ');
}

u8 decode_field(field field, u8 byte)
{
    u8 result = byte   << field.start_bit;
    result    = result >> field.start_bit;
    result    = result >> (7 - field.start_bit - field.length + 1);

    return result;
}

u8 match_instruction(u8 byte, instruction instruction)
{
    return (byte >> 8 - instruction.length) == instruction.opcode;
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

        if (match_instruction(first_byte, RegMemToFromReg)) {

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
            if (mod == 0b00000011) {

                strcpy(src, Registers[w][rm]);

            } else {
                if (mod == 0b00000000) {
                    if (rm == 0b00000110) {
                        strcpy(src, "que hacer????");
                        byte_index += 2;
                    } else {
                        strcpy(src, EffectiveAddresses[rm]);
                        strcat(src, "]");
                    }
                } else if (mod == 0b00000001) {

                    strcpy(src, EffectiveAddresses[rm]);

                    u8 disp = buffer[byte_index + 2];
                    if (disp) {
                        strcat(src, " + ");
                        sprintf(src + strlen(src), "%d", disp);
                    }
                    strcat(src, "]");

                    byte_index++;

                } else if (mod == 0b00000010) {

                    strcpy(src, EffectiveAddresses[rm]);

                    u8 disp_lo = buffer[byte_index + 2];
                    u8 disp_h  = buffer[byte_index + 3];
                    u16 disp = (u16)disp_h << 8 | (u16)disp_lo;
                    if (disp) {
                        strcat(src, " + ");
                        sprintf(src + strlen(src), "%d", disp);
                    }
                    strcat(src, "]");

                    byte_index += 2;

                }
            }

            if (!d) {
                printf("mov %s, %s\n", src, dest);
            } else {
                printf("mov %s, %s\n", dest, src);
            }


        } else if (match_instruction(first_byte, ImmToReg)) {

            field w_field   = { 1, 4 };
            field reg_field = { 3, 5 };

            u8 w   = decode_field(w_field, first_byte);
            u8 reg = decode_field(reg_field, first_byte);

            char * dest = Registers[w][reg];
            char src[32];
            if (w) {
                u8 disp_lo = buffer[byte_index + 1];
                u8 disp_h  = buffer[byte_index + 2];
                u16 disp = (u16)disp_h << 8 | (u16)disp_lo;
                sprintf(src, "%d", disp);

                byte_index++;
            } else {
                u8 disp = buffer[byte_index + 1];
                sprintf(src, "%d", disp);
            }

            printf("mov %s, %s\n", dest, src);

        }
    }

    free(buffer);
    
    return 0;
}
