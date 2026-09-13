#include <string.h>
#include <stdlib.h>
#include "encoder.h"
#include "opcodes.h"

uint32_t encode_load_weight(int row, int col, int data)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)LOAD_WEIGHT << 28);
    instruction |= ((uint32_t)row << 16);
    instruction |= ((uint32_t)col << 8);
    instruction |= ((uint32_t)data & 0xFF);

    return instruction;
}

uint32_t encode_load_act(int row, int data)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)LOAD_ACT << 28);
    instruction |= ((uint32_t)row << 16);
    instruction |= ((uint32_t)data & 0xFF);

    return instruction;
}

uint32_t encode_run(int clear_acc)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)RUN << 28);

    if(clear_acc)
    {
        instruction |= (1u << 24);
    }

    return instruction;
}

uint32_t encode_read_results(void)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)READ_RESULTS << 28);

    return instruction;
}

//0x5
uint32_t encode_set_mode(int add_sel)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)SET_MODE << 28);
    instruction |= ((uint32_t)add_sel & 0x1);

    return instruction;
}

//0x6
uint32_t encode_pipeline_instr(int opcode, int dst_reg, int src_reg, int src2_reg, int unit_id)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)opcode << 13);
    instruction |= ((uint32_t)dst_reg << 10);
    instruction |= ((uint32_t)src_reg << 7);
    instruction |= ((uint32_t)src2_reg << 4);
    instruction |= ((uint32_t)unit_id);

    return instruction;
}

uint32_t encode_load_instr(int address, int opcode, int dst_reg, int src_reg, int src2_reg, int unit_id)
{
    uint32_t instruction = 0;
    uint32_t pipeline = encode_pipeline_instr
                        (
                            opcode,
                            dst_reg,
                            src_reg,
                            src2_reg,
                            unit_id
                        );

    instruction |= ((uint32_t)LOAD_INSTR << 28);
    instruction |= ((uint32_t)address << 17);
    instruction |= pipeline;
    
    return instruction;
}

//0x7
uint32_t encode_run_program(int prog_len)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)RUN_PROGRAM << 28);
    instruction |= ((uint32_t)prog_len & 0xFF);

    return instruction;
}

//0x8
uint32_t encode_set_rescale(int shift)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)SET_RESCALE << 28);
    instruction |= ((uint32_t)shift & 0x1F);

    return instruction;
}

//0xA
uint32_t encode_clear_ssm_state(void)
{
    uint32_t instruciton = 0;
    
    instruciton |= ((uint32_t)CLEAR_SSM_STATE << 28);
    
    return instruciton;
}

//0xD
uint32_t encode_set_quant_params(int scale, int shift, int zero_point)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)SET_QUANT_PARAMS << 28);
    instruction |= ((uint32_t)scale << 13);
    instruction |= ((uint32_t)shift << 8);
    instruction |= ((uint32_t)zero_point << 0);
    
    return instruction;
}

//0xF
uint32_t encode_flush_weights(void)
{
    uint32_t instruciton = 0;

    instruciton |= ((uint32_t)FLUSH_WEIGHTS << 28);
    
    return instruciton;
}

int encode_instruction(const ParsedInstruction *instruction, uint32_t *encoded)
{
    int args[MAX_ARGS], i;

    if(instruction == NULL || encoded == NULL)
    {
        return 0;
    }

    for(i = 0; i < instruction->arg_count; i++)
    {
        char *end;
        long value = strtol(instruction->args[i], &end, 10);

        if(*end != '\0')
        {
            args[i] = 0;
        }else
        {
            args[i] = (int)value;
        }
    }

    if(strcmp(instruction->name, "LOAD_WEIGHT") == 0)
    {
        *encoded = encode_load_weight
                (
                    args[0],
                    args[1],
                    args[2]
                );
        return 1;
    }

    if(strcmp(instruction->name, "LOAD_ACT") == 0)
    {
        *encoded = encode_load_act
                (
                    args[0],
                    args[1]
                );

        return 1;
    }

    if(strcmp(instruction->name, "RUN") == 0)
    {
        *encoded = encode_run
                (
                    args[0]
                );

        return 1;
    }

    if(strcmp(instruction->name, "READ_RESULTS") == 0)
    {
        *encoded = encode_read_results();
        return 1;
    }

    if(strcmp(instruction->name, "SET_MODE") == 0)
    {
        *encoded = encode_set_mode
                (
                    args[0]
                );
        return 1;
    }

    //0x6
    if(strcmp(instruction->name, "LOAD_INSTR") == 0)
    {
        int address, opcode;
        int dst = 0;
        int src = 0;
        int src2 = 0;
        int unit = 0;

        char *end;
        long value = strtol(instruction->args[0], &end, 10);
        if(*end != '\0')
        {
            return 0;
        }

        address = (int)value;

        if(strcmp(instruction->args[1], "NOP") == 0)
        {
            opcode = PIPE_NOP;
        }else if(strcmp(instruction->args[1], "COMPUTE") == 0)
        {
            opcode = PIPE_COMPUTE;
            dst = instruction->args[2][1] - '0';
            src = instruction->args[3][1] - '0';

            if(strcmp(instruction->args[4], "RELU") == 0)
            {
                unit = UNIT_RELU;
            }else if(strcmp(instruction->args[4], "SIGMOID") == 0)
            {
                unit = UNIT_SIGMOID;
            }else if(strcmp(instruction->args[4], "SELU") == 0)
            {
                unit = UNIT_SELU;
            }else if(strcmp(instruction->args[4], "RMSNORM") == 0)
            {
                unit = UNIT_RMSNORM;
            }else if(strcmp(instruction->args[4], "ZSCORE") == 0)
            {
                unit = UNIT_ZSCORE;
            }else if(strcmp(instruction->args[4], "SOFTMAX") == 0)
            {
                unit = UNIT_SOFTMAX;
            }else if(strcmp(instruction->args[4], "ROPE") == 0)
            {
                unit = UNIT_ROPE;
            }else if(strcmp(instruction->args[4], "SSM_STEP") == 0)
            {
                unit = UNIT_SSM_STEP;
            }else if(strcmp(instruction->args[4], "BATCHNORM") == 0)
            {
                unit = UNIT_BATCHNORM;
            }else if(strcmp(instruction->args[4], "SWIGLU") == 0)
            {
                unit = UNIT_SWIGLU;
            }else if(strcmp(instruction->args[4], "QUANTIZE") == 0)
            {
                unit = UNIT_QUANTIZE;
            }else if(strcmp(instruction->args[4], "POOL_MAX") == 0)
            {
                unit = UNIT_POOL_MAX;
            }else if(strcmp(instruction->args[4], "POOL_MIN") == 0)
            {
                unit = UNIT_POOL_MIN;
            }else if(strcmp(instruction->args[4], "POOL_MEAN") == 0)
            {
                unit = UNIT_POOL_MEAN;
            }else
            {
                return 0;
            }
        }else if(strcmp(instruction->args[1], "STORE") == 0)
        {
            opcode = PIPE_STORE;
            dst = instruction->args[2][1] - '0';
            src = instruction->args[3][1] - '0';
        }else if(strcmp(instruction->args[1], "ADD") == 0)
        {
            opcode = PIPE_ADD;
            dst = instruction->args[2][1] - '0';
            src = instruction->args[3][1] - '0';
            src2 = instruction->args[4][1] - '0';
        }else if(strcmp(instruction->args[1], "HALT") == 0)
        {
            opcode = PIPE_HALT;
        }else
        {
            return 0;
        }

        *encoded = encode_load_instr(address, opcode, dst, src, src2, unit);

        return 1;
    }

    //0x7
    if(strcmp(instruction->name, "RUN_PROGRAM") == 0)
    {
        *encoded = encode_run_program
                (
                    args[0]
                );
        return 1;
    }

    //0x8
    if(strcmp(instruction->name, "SET_RESCALE") == 0)
    {
        *encoded = encode_set_rescale
                (
                    args[0]
                );
        return 1;
    }

    //0xA
    if(strcmp(instruction->name, "CLEAR_SSM_STATE") == 0)
    {
        *encoded = encode_clear_ssm_state();
        return 1;
    }

    //0xF
    if(strcmp(instruction->name, "FLUSH_WEIGHTS") == 0)
    {
        *encoded = encode_flush_weights();
        return 1;
    }
    
    return 0;
}