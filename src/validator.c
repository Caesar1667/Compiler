#include <string.h>
#include <stdlib.h>
#include "validator.h"
#include "opcodes.h"

int parse_pipeline(const char *token)
{
    if(!strcmp(token, "NOP"))
    {
        return PIPE_NOP;
    }
    if(!strcmp(token, "COMPUTE"))
    {
        return PIPE_COMPUTE;
    }
    if(!strcmp(token, "STORE"))
    {
        return PIPE_STORE;
    }
    if(!strcmp(token, "ADD"))
    {
        return PIPE_ADD;
    }
    if(!strcmp(token, "HALT"))
    {
        return PIPE_HALT;
    }
    return -1;
}

int parse_register(const char *token)
{
    if(token[0] != 'R')
    {
        return -1;
    }

    if(token[1] < '0' || token[1] > '7' || token[2] != '\0')
    {
        return -1;
    }

    return token[1] - '0';
}

int parse_unit_id(const char *token)
{
    if(!strcmp(token, "RELU"))
    {
        return UNIT_RELU;
    }
    if(!strcmp(token, "SIGMOID"))
    {
        return UNIT_SIGMOID;
    }
    if(!strcmp(token, "SELU"))
    {
        return UNIT_SELU;
    }
    if(!strcmp(token, "RMSNORM"))
    {
        return UNIT_RMSNORM;
    }
    if(!strcmp(token, "ZSCORE"))
    {
        return UNIT_ZSCORE;
    }
    if(!strcmp(token, "SOFTMAX"))
    {
        return UNIT_SOFTMAX;
    }
    if(!strcmp(token, "ROPE"))
    {
        return UNIT_ROPE;
    }
    if(!strcmp(token, "SSM_STEP"))
    {
        return UNIT_SSM_STEP;
    }
    if(!strcmp(token, "BATCHNORM"))
    {
        return UNIT_BATCHNORM;
    }
    if(!strcmp(token, "SWIGLU"))
    {
        return UNIT_SWIGLU;
    }
    if(!strcmp(token, "QUANTIZE"))
    {
        return UNIT_QUANTIZE;
    }
    if(!strcmp(token, "POOL_MAX"))
    {
        return UNIT_POOL_MAX;
    }
    if(!strcmp(token, "POOL_MIN"))
    {
        return UNIT_POOL_MIN;
    }
    if(!strcmp(token, "POOL_MEAN"))
    {
        return UNIT_POOL_MEAN;
    }

    return -1;
}

int parse_coef_selector(const char *token)
{
    if(!strcmp(token, "Abar"))
    {
        return COEF_ABAR;
    }
    if(!strcmp(token, "Bbar"))
    {
        return COEF_BBAR;
    }
    if(!strcmp(token, "C"))
    {
        return COEF_C;
    }
    if(!strcmp(token, "D"))
    {
        return COEF_D;
    }
    return -1;
}

int parse_param_selector(const char *token)
{
    if(!strcmp(token, "mean"))
    {
        return PARAM_MEAN;
    }
    if(!strcmp(token, "inv_std"))
    {
        return PARAM_INV_STD;
    }
    if(!strcmp(token, "gamma"))
    {
        return PARAM_GAMMA;
    }
    if(!strcmp(token, "beta"))
    {
        return PARAM_BETA;
    }
    return -1;
}

int parse_int(const char *token, int *value)
{
    char *end;
    long result;

    if(token == NULL || value == NULL)
    {
        return 0;
    }

    result = strtol(token, &end, 0);
    if(*end != '\0')
    {
        return 0;
    }

    *value = (int)result;
    return 1;
}

int validate_instruction(const ParsedInstruction *instruction)
{
    int value;

    if(instruction == NULL)
    {
        return 0;
    }

    if(!strcmp(instruction->name, "LOAD_WEIGHT"))
    {
        if(instruction->arg_count != 3)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= ROWS)
        {
            return 0;
        }
        if(!parse_int(instruction->args[1], &value) || value < 0 || value >= COLS)
        {
            return 0;
        }
        if(!parse_int(instruction->args[2], &value) || value < -128 || value > 127)
        {
            return 0;
        }

        return 1;
    }

    if(!strcmp(instruction->name, "LOAD_ACT"))
    {
        if(instruction->arg_count != 2)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= ROWS) 
        {
            return 0;
        }

        if(!parse_int(instruction->args[1], &value) || value < -128 || value > 127) 
        {
            return 0;
        }

        return 1;
    }

    if(!strcmp(instruction->name, "RUN"))
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || (value != 0 && value != 1))
        {
            return 0;
        }
        return 1;
    }
    
    if(!strcmp(instruction->name, "SET_MODE"))
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || (value != 0 && value != 1))
        {
            return 0;
        }
        return 1;
    }

    //0x6
    if(!strcmp(instruction->name, "LOAD_INSTR"))
    {
        int opcode, dst, src, src2, unit;

        if(instruction->arg_count < 2)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= PROG_DEPTH)
        {
            return 0;
        }

        opcode = parse_pipeline(instruction->args[1]);
        if(opcode == -1)
        {
            return 0;
        }

        if(opcode == PIPE_NOP || opcode == PIPE_HALT)
        {
            return (instruction->arg_count == 2);
        }

        if(opcode == PIPE_COMPUTE)
        {
            if(instruction->arg_count == 5)
            {
                dst = parse_register(instruction->args[2]);
                src = parse_register(instruction->args[3]);
                unit = parse_unit_id(instruction->args[4]);

                if(dst == -1 || src == -1 || unit == -1 || unit == UNIT_SWIGLU)
                {
                    return 0;
                }

                return 1;
            }else if(instruction->arg_count == 6)
            {
                dst = parse_register(instruction->args[2]);
                src = parse_register(instruction->args[3]);
                src2 = parse_register(instruction->args[4]);
                unit = parse_unit_id(instruction->args[5]);

                if(dst == -1 || src == -1 || src2 == -1 || unit != UNIT_SWIGLU)
                {
                    return 0;
                }
                return 1;
            }
            return 0;
        }

        if(opcode == PIPE_STORE)
        {
            if(instruction->arg_count != 4)
            {
                return 0;
            }
            dst = parse_register(instruction->args[2]);
            src = parse_register(instruction->args[3]);
            if(dst == -1 || src == -1)
            {
                return 0;
            }
            return 1;
        }

        if(opcode == PIPE_ADD)
        {
            if(instruction->arg_count != 5)
            {
                return 0;
            }
            dst = parse_register(instruction->args[2]);
            src = parse_register(instruction->args[3]);
            src2 = parse_register(instruction->args[4]);
            if(dst == -1 || src == -1 || src2 == -1)
            {
                return 0;
            }
            return 1;
        }
    }

    //0x7
    if(strcmp(instruction->name, "RUN_PROGRAM") == 0)
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value > PROG_DEPTH)
        {
            return 0;
        }
        return 1;
    }
    
    //0x8
    if(strcmp(instruction->name, "SET_RESCALE") == 0)
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || (value < 0 || value > 31))
        {
            return 0;
        }
        return 1;
    }

    
    //0x9
    if(strcmp(instruction->name, "LOAD_SSM_COEF") == 0)
    {
        if(instruction->arg_count != 3)
        {
            return 0;
        }

        if(strncmp(instruction->args[0], "which=", 6) != 0)
        {
            return 0;
        }
        if(parse_coef_selector(instruction->args[0] + 6) == -1)
        {
            return 0;
        }

        if(strncmp(instruction->args[1], "ch=", 3) != 0)
        {
            return 0;
        }
        if(!parse_int(instruction->args[1] + 3, &value) || value < 0 || value >= COLS)
        {
            return 0;
        }

        if(!parse_int(instruction->args[2], &value) || value < -32768 || value > 32767)
        {
            return 0;
        }

        return 1;
    }

    //0xA
    if(strcmp(instruction->name, "CLEAR_SSM_STATE") == 0)
    {
        return (instruction->arg_count == 0);
    }

    //0xB
    if(strcmp(instruction->name, "SET_ROPE_POS") == 0)
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= N_POS)
        {
            return 0;
        }

        return 1;
    }

    //0xC
    if(strcmp(instruction->name, "LOAD_BN_PARAM") == 0)
    {
        if(instruction->arg_count != 3)
        {
            return 0;
        }

        if(strncmp(instruction->args[0], "which=", 6) != 0)
        {
            return 0;
        }
        if(parse_param_selector(instruction->args[0] + 6) == -1)
        {
            return 0;
        }

        if(strncmp(instruction->args[1], "ch=", 3) != 0)
        {
            return 0;
        }
        if(!parse_int(instruction->args[1] + 3, &value) || value < 0 || value >= COLS)
        {
            return 0;
        }

        if(!parse_int(instruction->args[2], &value) || value < -32768 || value > 32767)
        {
            return 0;
        }

        return 1;
    }

    //0xD
    if(strcmp(instruction->name, "SET_QUANT_PARAMS") == 0)
    {
        if(instruction->arg_count != 3)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < -16384 || value > 16383)
        {
            return 0;
        }

        if(!parse_int(instruction->args[1], &value) || value < 0 || value > 31)
        {
            return 0;
        }

        if(!parse_int(instruction->args[2], &value) || value < -128 || value > 127)
        {
            return 0;
        }

        return 1;
    }

    //0xE
    if(strcmp(instruction->name, "LOAD_WEIGHT_BURST") == 0)
    {
        if(instruction->arg_count != 2)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= (ROWS*COLS))
        {
            return 0;
        }

        if(!parse_int(instruction->args[1], &value) || value < -128 || value > 127)
        {
            return 0;
        }

        return 1;
    }

    //0xF
    if(strcmp(instruction->name, "FLUSH_WEIGHTS") == 0)
    {
        return (instruction->arg_count == 0);
    }

    return 0;
}
