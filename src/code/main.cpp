#include "main.h"

static long read_file(u8 **buffer, char *file_name, char *mode)
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

static u8 decode_field(field field, u8 byte)
{
    u8 result = byte   << field.start_bit;
    result    = result >> field.start_bit;
    result    = result >> (7 - field.start_bit - field.length + 1);

    return result;
}

static u8 match_instruction(u8 byte, inst_code inst_code)
{
    return (byte >> 8 - inst_code.length) == inst_code.opcode;
}

static u16 get_u16_at(u8 *buffer)
{
    u8 disp_lo = buffer[0];
    u8 disp_h  = buffer[1];
    return (u16)disp_h << 8 | (u16)disp_lo;
}

static size_t decode_address_or_register(u8 *buffer, char *result, u8 w, u8 mod, u8 rm)
{
    size_t offset = 0;
    switch (mod)
    {
        case 0b00000011: { strcpy(result, RegisterNames[w][rm]); } break;

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

static size_t decode_instruction(u8 *buffer, size_t byte_index, instruction *instruction, u16 *registers)
{
    u8 first_byte  = buffer[byte_index];
    u8 second_byte = buffer[byte_index + 1];

    if (match_instruction(first_byte, MovRegMemToFromReg))
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

        if (!d)
        {
            registers[rm] = registers[reg];
            byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->dest, w, mod, rm);
            sprintf(instruction->src, "%s", RegisterNames[w][reg]);
        }
        else
        {
            registers[reg] = registers[rm];
            byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);
        }

        instruction->type = instruction_type::mov;
    }
    else if (match_instruction(first_byte, MovImmToRegMem))
    {
        field w_field   = { 1, 7 };
        field mod_field = { 2, 0 };
        field rm_field  = { 3, 5 };

        u8 w   = decode_field(w_field,   first_byte);
        u8 mod = decode_field(mod_field, second_byte);
        u8 rm  = decode_field(rm_field,  second_byte);

        byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->dest, w, mod, rm);

        if (w)
        {
            u16 imm = get_u16_at(&buffer[byte_index + 2]);
            sprintf(instruction->src, " word %d", imm);
            byte_index += 2;
        }
        else
        {
            u8 imm = buffer[byte_index + 2];
            sprintf(instruction->src, " byte %d", imm);
            byte_index++;
        }

        instruction->type = instruction_type::mov;
    }
    else if (match_instruction(first_byte, MovImmToReg))
    {
        field w_field   = { 1, 4 };
        field reg_field = { 3, 5 };

        u8 w   = decode_field(w_field, first_byte);
        u8 reg = decode_field(reg_field, first_byte);

        sprintf(instruction->dest, "%s", RegisterNames[w][reg]);

        if (w)
        {
            u16 imm = get_u16_at(&buffer[byte_index + 1]);
            registers[reg] = imm;

            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
        else
        {
            u8 imm = buffer[byte_index + 1];
            sprintf(instruction->src, "%d", imm);
        }

        instruction->type = instruction_type::mov;
    }
    else if (match_instruction(first_byte, AddRegMemWithRegToEither))
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

        if (!d)
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->dest, w, mod, rm);
            sprintf(instruction->src, "%s", RegisterNames[w][reg]);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);
        }

        instruction->type = instruction_type::add;
    }
    else if (match_instruction(first_byte, SubRegMemWithRegToEither))
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

        char src[32];
        if (!d)
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->dest, w, mod, rm);
            sprintf(instruction->src, "%s", RegisterNames[w][reg]);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);
        }

        instruction->type = instruction_type::sub;
    }
    else if (match_instruction(first_byte, CmpRegMemWithRegToEither))
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

        if (!d)
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->dest, w, mod, rm);
            sprintf(instruction->src, "%s", RegisterNames[w][reg]);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);
        }

        instruction->type = instruction_type::cmp;
    }
    else if (match_instruction(first_byte, AddImmToAcc))
    {
        field w_field = { 1, 7 };
        u8 w = decode_field(w_field, first_byte);

        if (w)
        { 
            s16 imm = (s16)get_u16_at(&buffer[byte_index + 1]);
            sprintf(instruction->dest, "ax");
            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);
        }

        instruction->type = instruction_type::add;
    }
    else if (match_instruction(first_byte, SubImmFromAcc))
    {
        field w_field = { 1, 7 };
        u8 w = decode_field(w_field,   first_byte);

        if (w)
        { 
            s16 imm = (s16)get_u16_at(&buffer[byte_index + 1]);
            sprintf(instruction->dest, "ax");
            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);
        }

        instruction->type = instruction_type::sub;
    }
    else if (match_instruction(first_byte, CmpImmWithAcc))
    {
        field w_field = { 1, 7 };
        u8 w = decode_field(w_field, first_byte);

        if (w)
        { 
            s16 imm = (s16)get_u16_at(&buffer[byte_index + 1]);
            sprintf(instruction->dest, "ax");
            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);
        }

        instruction->type = instruction_type::cmp;
    }
    else if (match_instruction(first_byte, AddSubCmpImmXRegMem))
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
            instruction->type = instruction_type::add;
        }
        else if (op == 0b00000101)
        {
            instruction->type = instruction_type::sub;
        }
        else if (op == 0b00000111)
        {
            instruction->type = instruction_type::cmp;
        }

        if (w)
        {
            s16 imm;
            if (!s)
            {
                imm = (s16)get_u16_at(&buffer[byte_index + 2]);
                byte_index += 2;
            }
            else
            {
                imm = (s16)buffer[byte_index + 2];
                byte_index++;
            }
            sprintf(instruction->dest, "word %s", dest);
            sprintf(instruction->src, "%d", imm);
        }
        else
        {
            s8 imm = buffer[byte_index + 2];
            sprintf(instruction->dest, "byte %s", dest);
            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
    }
    else if (match_instruction(first_byte, JNZ))
    {
        printf("jnz label\n");
    }
    else if (match_instruction(first_byte, JE))
    {
        printf("je label\n");
    }
    else if (match_instruction(first_byte, JLE))
    {
        printf("jle label\n");
    }
    else if (match_instruction(first_byte, JB))
    {
        printf("jb label\n");
    }
    else if (match_instruction(first_byte, JBE))
    {
        printf("jbe label\n");
    }
    else if (match_instruction(first_byte, JP))
    {
        printf("jp label\n");
    }
    else if (match_instruction(first_byte, JO))
    {
        printf("jo label\n");
    }
    else if (match_instruction(first_byte, JS))
    {
        printf("js label\n");
    }
    else if (match_instruction(first_byte, JNE))
    {
        printf("jne label\n");
    }
    else if (match_instruction(first_byte, JNL))
    {
        printf("jnl label\n");
    }
    else if (match_instruction(first_byte, JG))
    {
        printf("jg label\n");
    }
    else if (match_instruction(first_byte, JNB))
    {
        printf("jnb label\n");
    }
    else if (match_instruction(first_byte, JA))
    {
        printf("ja label\n");
    }
    else if (match_instruction(first_byte, JNP))
    {
        printf("jnp label\n");
    }
    else if (match_instruction(first_byte, JNO))
    {
        printf("jno label\n");
    }
    else if (match_instruction(first_byte, JNS))
    {
        printf("jns label\n");
    }
    else if (match_instruction(first_byte, LOOP))
    {
        printf("loop label\n");
    }
    else if (match_instruction(first_byte, LOOPZ))
    {
        printf("loopz label\n");
    }
    else if (match_instruction(first_byte, LOOPNZ))
    {
        printf("loopnz label\n");
    }
    else if (match_instruction(first_byte, JCXZ))
    {
        printf("jcxz label\n");
    }

    return byte_index;
}

static void print_instruction(instruction *instruction)
{
    switch (instruction->type)
    {
        case (instruction_type::mov):
        {
            printf("mov ");
        } break;
        case (instruction_type::add):
        {
            printf("add ");
        } break;
        case (instruction_type::sub):
        {
            printf("sub ");
        } break;
        case (instruction_type::cmp):
        {
            printf("cmp ");
        } break;
    };

    printf("%s, ", instruction->dest);
    printf("%s\n", instruction->src);
}

int main(int argc, char *argv[])
{
    // Speed(Fermin): Instead of many prints, would it be 
    // faster to write to a buffer and just print once?
    printf("bits 16\n\n");

    u8 *buffer = {};
    long file_size = read_file(&buffer, argv[1], (char *)"rb");

    u16 registers[8] = {};

    for (size_t byte_index = 0; byte_index < file_size; byte_index += 2)
    {
        instruction instruction = {};
        byte_index = decode_instruction(buffer, byte_index, &instruction, registers);
        print_instruction(&instruction);
    }

    printf("\nPrinting Registers\n");
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::ax], registers[RegisterIndex::ax]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::bx], registers[RegisterIndex::bx]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::cx], registers[RegisterIndex::cx]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::dx], registers[RegisterIndex::dx]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::sp], registers[RegisterIndex::sp]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::bp], registers[RegisterIndex::bp]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::si], registers[RegisterIndex::si]);
    printf("%s: %d\n", RegisterNames[1][RegisterIndex::di], registers[RegisterIndex::di]);

    free(buffer);
    
    return 0;
}
