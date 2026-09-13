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

        if(instruction->args[0] < 0 || instruction->args[0] >= ROWS)
        {
            return 0;
        }

        if(instruction->args[1] < 0 || instruction->args[1] >= COLS)
        {
            return 0;
        }

        if(instruction->args[2] < -128 || instruction->args[2] > 127)
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

        if(instruction->args[0] < 0 || instruction->args[0] >= ROWS)
        {
            return 0;
        }

        if(instruction->args[1] < -128 || instruction->args[1] > 127)
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

        if(instruction->args[0] != 0 && instruction->args[0] != 1)
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

        if(instruction->args[0] != 0 && instruction->args[0] != 1)
        {
            return 0;
        }
        return 1;
    }

    //0x7
    if(strcmp(instruction->name, "RUN_PROGRAM") == 0)
    {
        if(instruction->arg_count != 1)
        {
            return 0;
        }

        if(instruction->args[0] < 0 || instruction->args[0] >31)
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

        if(instruction->args[0] < 0 || instruction->args[0] > 31)
        {
            return 0;
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
