#include <string.h>
#include <stdlib.h>
#include "validator.h"
#include "opcodes.h"

int parse_pipeline(const char *token)
{
    if(strcmp(token, "NOP") == 0)
    {
        return PIPE_NOP;
    }
    if(strcmp(token, "COMPUTE") == 0)
    {
        return PIPE_COMPUTE;
    }
    if(strcmp(token, "STORE") == 0)
    {
        return PIPE_STORE;
    }
    if(strcmp(token, "ADD") == 0)
    {
        return PIPE_ADD;
    }
    if(strcmp(token, "HALT") == 0)
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
    if(strcmp(token, "RELU") == 0)
    {
        return UNIT_RELU;
    }
    if(strcmp(token, "SIGMOID") == 0)
    {
        return UNIT_SIGMOID;
    }
    if(strcmp(token, "SELU") == 0)
    {
        return UNIT_SELU;
    }
    if(strcmp(token, "RMSNORM") == 0)
    {
        return UNIT_RMSNORM;
    }
    if(strcmp(token, "ZSCORE") == 0)
    {
        return UNIT_ZSCORE;
    }
    if(strcmp(token, "SOFTMAX") == 0)
    {
        return UNIT_SOFTMAX;
    }
    if(strcmp(token, "ROPE") == 0)
    {
        return UNIT_ROPE;
    }
    if(strcmp(token, "SSM_STEP") == 0)
    {
        return UNIT_SSM_STEP;
    }
    if(strcmp(token, "BATCHNORM") == 0)
    {
        return UNIT_BATCHNORM;
    }
    if(strcmp(token, "SWIGLU") == 0)
    {
        return UNIT_SWIGLU;
    }
    if(strcmp(token, "QUANTIZE") == 0)
    {
        return UNIT_QUANTIZE;
    }
    if(strcmp(token, "POOL_MAX") == 0)
    {
        return UNIT_POOL_MAX;
    }
    if(strcmp(token, "POOL_MIN") == 0)
    {
        return UNIT_POOL_MIN;
    }
    if(strcmp(token, "POOL_MEAN") == 0)
    {
        return UNIT_POOL_MEAN;
    }

    return -1;
}

int parse_ssm_coef_selector(const char *token)
{
    if(strcmp(token, "ABAR") == 0)
    {
        return 0;
    }else if(strcmp(token, "BBAR") == 0)
    {
        return 1;
    }else if(strcmp(token, "C") == 0)
    {
        return 2;
    }else if(strcmp(token, "D") == 0)
    {
        return 3;
    }else
    {
        return -1;
    }
}

int parse_int(const char *token, int *value)
{
    char *end;
    long result;
    result = strtol(token, &end, 10);
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

    if(strcmp(instruction->name, "LOAD_WEIGHT") == 0)
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
        if(!parse_int(instruction->args[2], &value) || value < -128 || value >= 127)
        {
            return 0;
        }

        return 1;
    }

    if(strcmp(instruction->name, "LOAD_ACT") == 0)
    {
        if(instruction->arg_count != 2)
        {
            return 0;
        }

        if(!parse_int(instruction->args[0], &value) || value < 0 || value >= ROWS) 
        {
            return 0;
        }

        if(!parse_int(instruction->args[1], &value) || value < -128 || value >= 127) 
        {
            return 0;
        }

        return 1;
    }

    if(strcmp(instruction->name, "RUN") == 0)
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
    
    if(strcmp(instruction->name, "READ_RESULTS") == 0)
    {
        if(instruction->arg_count !=  0)
        {
            return 0;
        }
        
        return 1;
    }
    
    if(strcmp(instruction->name, "SET_MODE") == 0)
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
    if(strcmp(instruction->name, "LOAD_INSTR") == 0)
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
            if(instruction->arg_count != 2)
            {
                return 0;
            }
            return 1;
        }

        if(opcode == PIPE_COMPUTE)
        {
            if(instruction->arg_count != 5)
            {
                return 0;
            }

            dst = parse_register(instruction->args[2]);
            src = parse_register(instruction->args[3]);
            unit = parse_unit_id(instruction->args[4]);

            if(dst == -1 || src == -1 || unit == -1)
            {
                return 0;
            }

            return 1;
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
        return 0;
    }

    //0x7
    if(strcmp(instruction->name, "RUN_PROGRAM") == 0)
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
    if(strcmp(instruction->name, "LOAD_SSM_COEF"))
    {
        if(instruction->arg_count != 3)
        {
            return 0;
        }

        int coef_selector = parse_ssm_coef_selector(instruction->args[0]);
        int channel, coefficient;
        if(coef_selector == -1)
        {
            return -1;
        }

        if(!parse_int(instruction->args[1], &channel) || channel < 0 || channel > 31)
        {
            return -1;
        }

        if(!parse_int(instruction->args[2], &coefficient) || coefficient < -32768 || coefficient > 32767)
        {
            return -1;
        }

        return 1;
    }

    //0xA
    if(strcmp(instruction->name, "CLEAR_SSM_STATE") == 0)
    {
        if(instruction->arg_count !=  0)
        {
            return 0;
        }
        
        return 1;
    }

    //0xF
    if(strcmp(instruction->name, "FLUSH_WEIGHTS") == 0)
    {
        if(instruction->arg_count !=  0)
        {
            return 0;
        }
        
        return 1;
    }

    return 0;
}
