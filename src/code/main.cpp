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

static void check_and_set_zero_flag(u8 *flags, u16 value) {

    if(value == 0)
    {
        *flags |= instruction_flag::zero;
    }
    else
    {
        *flags &= ~instruction_flag::zero;
    }
}

static void check_and_set_sign_flag(u8 *flags, u16 value) {

    u16 high_bit = 0x8000;

    if((u16)(value & high_bit) == high_bit)
    {
        *flags |= instruction_flag::sign;
    }
    else
    {
        *flags &= ~instruction_flag::sign;
    }
}

static size_t decode_instruction(
    u8 *buffer,
    size_t byte_index,
    instruction *instruction,
    u16 *registers,
    u8 *flags
) {
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

            registers[rm] = registers[rm] + registers[reg];

            check_and_set_zero_flag(flags, registers[rm]);
            check_and_set_sign_flag(flags, registers[rm]);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                             instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);

            registers[reg] = registers[reg] + registers[rm];
            
            check_and_set_zero_flag(flags, registers[reg]);
            check_and_set_sign_flag(flags, registers[reg]);
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

            registers[rm] = registers[rm] - registers[reg];
            
            check_and_set_zero_flag(flags, registers[rm]);
            check_and_set_sign_flag(flags, registers[rm]);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);

            registers[reg] = registers[reg] - registers[rm];
            
            check_and_set_zero_flag(flags, registers[reg]);
            check_and_set_sign_flag(flags, registers[reg]);
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

            u16 result = registers[rm] - registers[reg];
            
            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);
        }
        else
        {
            byte_index += decode_address_or_register(&buffer[byte_index],
                                 instruction->src, w, mod, rm);
            sprintf(instruction->dest, "%s", RegisterNames[w][reg]);

            u16 result = registers[reg] - registers[rm];
            
            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);
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

            registers[register_index::ax] += imm;
            check_and_set_zero_flag(flags, registers[register_index::ax]);
            check_and_set_sign_flag(flags, registers[register_index::ax]);

            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);

            registers[register_index::ax] += (u16)imm;
            check_and_set_zero_flag(flags, registers[register_index::ax]);
            check_and_set_sign_flag(flags, registers[register_index::ax]);
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

            registers[register_index::ax] -= imm;
            check_and_set_zero_flag(flags, registers[register_index::ax]);
            check_and_set_sign_flag(flags, registers[register_index::ax]);

            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);

            registers[register_index::ax] -= imm;
            check_and_set_zero_flag(flags, registers[register_index::ax]);
            check_and_set_sign_flag(flags, registers[register_index::ax]);
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

            u16 result = registers[register_index::ax] - imm;
            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);

            byte_index++;
        }
        else
        {
            s8 imm = buffer[byte_index + 1];
            sprintf(instruction->dest, "al");
            sprintf(instruction->src, "%d", imm);

            u16 result = registers[register_index::ax] - (u16)imm;
            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);
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

            u16 result = 0;
            if (op == 0b00000000)
            {
                instruction->type = instruction_type::add;
                result = registers[rm] + imm;
                registers[rm] = result;
            }
            else if (op == 0b00000101)
            {
                instruction->type = instruction_type::sub;
                result = registers[rm] - imm;
                registers[rm] = result;
            }
            else if (op == 0b00000111)
            {
                instruction->type = instruction_type::cmp;
                result = registers[rm] - imm;
            }

            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);

            sprintf(instruction->dest, "word %s", dest);
            sprintf(instruction->src, "%d", imm);
        }
        else
        {
            s8 imm = buffer[byte_index + 2];

            u16 result = 0;
            if (op == 0b00000000)
            {
                instruction->type = instruction_type::add;
                result = registers[rm] + (u16)imm;
                registers[rm] = result;
            }
            else if (op == 0b00000101)
            {
                instruction->type = instruction_type::sub;
                result = registers[rm] - (u16)imm;
                registers[rm] = result;
            }
            else if (op == 0b00000111)
            {
                instruction->type = instruction_type::cmp;
                result = registers[rm] - (u16)imm;
            }

            check_and_set_zero_flag(flags, result);
            check_and_set_sign_flag(flags, result);

            sprintf(instruction->dest, "byte %s", dest);
            sprintf(instruction->src, "%d", imm);
            byte_index++;
        }
    }
    else if (match_instruction(first_byte, JNZ))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "$%d", offset + 2);
        instruction->type = instruction_type::jnz;

        if (!(*flags & instruction_flag::zero))
        {
            byte_index += offset;
        }
    }
    else if (match_instruction(first_byte, JE))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::je;
    }
    else if (match_instruction(first_byte, JLE))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jle;
    }
    else if (match_instruction(first_byte, JB))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jb;
    }
    else if (match_instruction(first_byte, JBE))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jbe;
    }
    else if (match_instruction(first_byte, JP))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jp;
    }
    else if (match_instruction(first_byte, JO))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jo;
    }
    else if (match_instruction(first_byte, JS))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::js;
    }
    else if (match_instruction(first_byte, JNE))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jne;
    }
    else if (match_instruction(first_byte, JNL))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jnl;
    }
    else if (match_instruction(first_byte, JG))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jg;
    }
    else if (match_instruction(first_byte, JNB))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jnb;
    }
    else if (match_instruction(first_byte, JA))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::ja;
    }
    else if (match_instruction(first_byte, JNP))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jnp;
    }
    else if (match_instruction(first_byte, JNO))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jno;
    }
    else if (match_instruction(first_byte, JNS))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jns;
    }
    else if (match_instruction(first_byte, LOOP))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::loop;
    }
    else if (match_instruction(first_byte, LOOPZ))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::loopz;
    }
    else if (match_instruction(first_byte, LOOPNZ))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::loopnz;
    }
    else if (match_instruction(first_byte, JCXZ))
    {
        s8 offset = buffer[byte_index + 1];
        sprintf(instruction->dest, "%d", offset);
        instruction->type = instruction_type::jcxz;
    }

    return byte_index;
}

static void print_instruction(instruction *instruction)
{
    printf("%s ", InstructionMnemonics[instruction->type]);
    printf("%s", instruction->dest);
    if (instruction->src[0] != '\0')
    {
        printf(", %s", instruction->src);
    }
}

static void print_registers(u16 *registers, u8 flags, u8 ip)
{

    printf("\nFinal Registers\n");
    printf("%s: %d\n", RegisterNames[1][register_index::ax], registers[register_index::ax]);
    printf("%s: %d\n", RegisterNames[1][register_index::bx], registers[register_index::bx]);
    printf("%s: %d\n", RegisterNames[1][register_index::cx], registers[register_index::cx]);
    printf("%s: %d\n", RegisterNames[1][register_index::dx], registers[register_index::dx]);
    printf("%s: %d\n", RegisterNames[1][register_index::sp], registers[register_index::sp]);
    printf("%s: %d\n", RegisterNames[1][register_index::bp], registers[register_index::bp]);
    printf("%s: %d\n", RegisterNames[1][register_index::si], registers[register_index::si]);
    printf("%s: %d\n", RegisterNames[1][register_index::di], registers[register_index::di]);

    printf("\n");
    printf("ip: %d\n", ip);
    printf("zero flag: %d\n", (flags & instruction_flag::zero) >> 0);
    printf("sign flag: %d\n", (flags & instruction_flag::sign) >> 1);
}

int main(int argc, char *argv[])
{
    printf("bits 16\n\n");

    u8 *buffer = {};
    long file_size = read_file(&buffer, argv[1], (char *)"rb");

    u16 registers[8] = {};
    u8 flags = 0;
    size_t ip_before = 0;

    for (size_t byte_index = 0; byte_index < file_size; byte_index += 2)
    {
        instruction instruction = {};
        ip_before = byte_index;

        byte_index = decode_instruction(buffer, byte_index, &instruction, registers, &flags);

        print_instruction(&instruction);
        printf(" ; ip:%zd->%zd", ip_before, byte_index + 2);
        printf("\n");
    }

    size_t final_ip = ip_before + 2;
    print_registers(registers, flags, final_ip);

    free(buffer);
    
    return 0;
}
