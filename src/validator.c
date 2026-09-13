#include <string.h>
#include "validator.h"
#include "opcodes.h"

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
