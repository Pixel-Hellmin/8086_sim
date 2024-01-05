#include "main.h"

long read_file(u8 **buffer, char *file_name, char *mode)
{
    FILE *file = {};
    if (fopen_s(&file, file_name, mode) != 0)
    {
        fprintf(stderr, "ERROR: Unable to open %s.\n", file_name);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    *buffer = (u8 *)malloc(file_size);

    size_t bytes_read = fread(*buffer, 1, file_size, file);
    fclose(file);

    return bytes_read;
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

u16 get_u16_at(u8 *buffer)
{
    u8 disp_lo = buffer[0];
    u8 disp_h  = buffer[1];
    return (u16)disp_h << 8 | (u16)disp_lo;
}

size_t decode_address_or_register(u8 *buffer, char *result, u8 w, u8 mod, u8 rm)
{
    size_t offset = 0;
    switch (mod)
    {
        case 0b00000011: { strcpy(result, Registers[w][rm]); } break;

        case 0b00000000:
        {
            if (rm == 0b00000110)
            {
                u16 disp = get_u16_at(&buffer[2]);
                if (disp)
                {
                    strcpy(result, "[");
                    sprintf(result + strlen(result), "%d", disp);
                    strcat(result, "]");
                }
                offset += 2;
            }
            else
            {
                strcpy(result, EffectiveAddresses[rm]);
                strcat(result, "]");
            }
        } break;

        case 0b00000001:
        {
            strcpy(result, EffectiveAddresses[rm]);
            u8 disp = buffer[2];
            if (disp)
            {
                strcat(result, " + ");
                sprintf(result + strlen(result), "%d", disp);
            }
            strcat(result, "]");
            offset++;
        } break;

        case 0b00000010:
        {
            strcpy(result, EffectiveAddresses[rm]);
            u16 disp = get_u16_at(&buffer[2]);
            if (disp)
            {
                strcat(result, " + ");
                sprintf(result + strlen(result), "%d", disp);
            }
            strcat(result, "]");
            offset += 2;
        } break;
    }

    return offset;
}

int main(int argc, char *argv[])
{
    // Speed(Fermin): Instead of many prints, would it be 
    // faster to write to a buffer and just print once?
    printf("bits 16\n\n");

    u8 *buffer = {};
    long file_size = read_file(&buffer, argv[1], (char *)"rb");

    for (size_t byte_index = 0; byte_index < file_size; byte_index += 2)
    {
        u8 first_byte  = buffer[byte_index];
        u8 second_byte = buffer[byte_index + 1];

        if (decode_instruction(first_byte, MovRegMemToFromReg))
        {
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

            if (!d)
            {
                printf("mov %s, %s\n", src, dest);
            }
            else
            {
                printf("mov %s, %s\n", dest, src);
            }
        }
        else if (decode_instruction(first_byte, MovImmToRegMem))
        {
            field w_field   = { 1, 7 };
            field mod_field = { 2, 0 };
            field rm_field  = { 3, 5 };

            u8 w   = decode_field(w_field,   first_byte);
            u8 mod = decode_field(mod_field, second_byte);
            u8 rm  = decode_field(rm_field,  second_byte);

            char dest[32];
            byte_index += decode_address_or_register(&buffer[byte_index],
                                                     dest, w, mod, rm);

            printf("mov %s,", dest);

            if (w)
            {
                u16 imm = get_u16_at(&buffer[byte_index + 2]);
                printf(" word %d\n", imm);
                byte_index += 2;
            }
            else
            {
                u8 imm = buffer[byte_index + 2];
                printf(" byte %d\n", imm);
                byte_index++;
            }
        }
        else if (decode_instruction(first_byte, MovImmToReg))
        {
            field w_field   = { 1, 4 };
            field reg_field = { 3, 5 };

            u8 w   = decode_field(w_field, first_byte);
            u8 reg = decode_field(reg_field, first_byte);

            char * dest = Registers[w][reg];
            printf("mov %s,", dest);

            if (w)
            {
                u16 imm = get_u16_at(&buffer[byte_index + 1]);
                printf("%d\n", imm);
                byte_index++;
            }
            else
            {
                u8 imm = buffer[byte_index + 1];
                printf("%d\n", imm);
            }
        }
        else if (decode_instruction(first_byte, AddRegMemWithRegToEither))
        {
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

            if (!d)
            {
                printf("add %s, %s\n", src, dest);
            }
            else
            {
                printf("add %s, %s\n", dest, src);
            }
        }
        else if (decode_instruction(first_byte, SubRegMemWithRegToEither))
        {
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
            if (!d)
            {
                printf("sub %s, %s\n", src, dest);
            }
            else
            {
                printf("sub %s, %s\n", dest, src);
            }
        }
        else if (decode_instruction(first_byte, CmpRegMemWithRegToEither))
        {
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

            if (!d)
            {
                printf("cmp %s, %s\n", src, dest);
            }
            else
            {
                printf("cmp %s, %s\n", dest, src);
            }
        }
        else if (decode_instruction(first_byte, AddImmToAcc))
        {
            field w_field = { 1, 7 };
            u8 w = decode_field(w_field, first_byte);

            printf("add ");

            if (w)
            { 
                i16 imm = (i16)get_u16_at(&buffer[byte_index + 1]);
                printf("ax, %d\n", imm);
                byte_index++;
            }
            else
            {
                i8 imm = buffer[byte_index + 1];
                printf("al, %d\n", imm);
            }
        }
        else if (decode_instruction(first_byte, SubImmFromAcc))
        {
            field w_field = { 1, 7 };
            u8 w = decode_field(w_field,   first_byte);

            printf("sub ");

            if (w)
            { 
                i16 imm = (i16)get_u16_at(&buffer[byte_index + 1]);
                printf("ax, %d\n", imm);
                byte_index++;
            }
            else
            {
                i8 imm = buffer[byte_index + 1];
                printf("al, %d\n", imm);
            }
        }
        else if (decode_instruction(first_byte, CmpImmWithAcc))
        {
            field w_field = { 1, 7 };
            u8 w = decode_field(w_field, first_byte);

            printf("cmp ");

            if (w)
            { 
                i16 imm = (i16)get_u16_at(&buffer[byte_index + 1]);
                printf("ax, %d\n", imm);
                byte_index++;
            }
            else
            {
                i8 imm = buffer[byte_index + 1];
                printf("al, %d\n", imm);
            }
        }
        else if (decode_instruction(first_byte, AddSubCmpImmXRegMem))
        {
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

            if (op == 0b00000000)
            {
                printf("add");
            }
            else if (op == 0b00000101)
            {
                printf("sub");
            }
            else if (op == 0b00000111)
            {
                printf("cmp");
            }

            if (w)
            {
                i16 imm;
                if (!s)
                {
                    imm = (i16)get_u16_at(&buffer[byte_index + 2]);
                    byte_index += 2;
                }
                else
                {
                    imm = (i16)buffer[byte_index + 2];
                    byte_index++;
                }
                printf(" word %s, %d\n", dest, imm);
            }
            else
            {
                i8 imm = buffer[byte_index + 2];
                printf(" byte %s, %d\n", dest, imm);
                byte_index++;
            }
        }
        else if (decode_instruction(first_byte, JNZ))
        {
            printf("jnz label\n");
        }
        else if (decode_instruction(first_byte, JE))
        {
            printf("je label\n");
        }
        else if (decode_instruction(first_byte, JLE))
        {
            printf("jle label\n");
        }
        else if (decode_instruction(first_byte, JB))
        {
            printf("jb label\n");
        }
        else if (decode_instruction(first_byte, JBE))
        {
            printf("jbe label\n");
        }
        else if (decode_instruction(first_byte, JP))
        {
            printf("jp label\n");
        }
        else if (decode_instruction(first_byte, JO))
        {
            printf("jo label\n");
        }
        else if (decode_instruction(first_byte, JS))
        {
            printf("js label\n");
        }
        else if (decode_instruction(first_byte, JNE))
        {
            printf("jne label\n");
        }
        else if (decode_instruction(first_byte, JNL))
        {
            printf("jnl label\n");
        }
        else if (decode_instruction(first_byte, JG))
        {
            printf("jg label\n");
        }
        else if (decode_instruction(first_byte, JNB))
        {
            printf("jnb label\n");
        }
        else if (decode_instruction(first_byte, JA))
        {
            printf("ja label\n");
        }
        else if (decode_instruction(first_byte, JNP))
        {
            printf("jnp label\n");
        }
        else if (decode_instruction(first_byte, JNO))
        {
            printf("jno label\n");
        }
        else if (decode_instruction(first_byte, JNS))
        {
            printf("jns label\n");
        }
        else if (decode_instruction(first_byte, LOOP))
        {
            printf("loop label\n");
        }
        else if (decode_instruction(first_byte, LOOPZ))
        {
            printf("loopz label\n");
        }
        else if (decode_instruction(first_byte, LOOPNZ))
        {
            printf("loopnz label\n");
        }
        else if (decode_instruction(first_byte, JCXZ))
        {
            printf("jcxz label\n");
        }
    }

    free(buffer);
    
    return 0;
}
