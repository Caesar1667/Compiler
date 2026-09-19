#include <string.h>
#include <stdlib.h>
#include "encoder.h"
#include "opcodes.h"
#include "validator.h"

uint32_t encode_load_weight(int row, int col, int data)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)LOAD_WEIGHT & 0xF) << 28);
    instruction |= (((uint32_t)row & 0x1F) << 16);
    instruction |= (((uint32_t)col & 0x1F) << 8);
    instruction |= ((uint32_t)data & 0xFF);

    return instruction;
}

uint32_t encode_load_act(int row, int data)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)LOAD_ACT & 0xF) << 28);
    instruction |= (((uint32_t)row & 0x1F) << 16);
    instruction |= ((uint32_t)data & 0xFF);

    return instruction;
}

uint32_t encode_run(int clear_acc)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)RUN & 0xF) << 28);

    if(clear_acc)
    {
        instruction |= (1u << 24);
    }

    return instruction;
}

uint32_t encode_read_results(void)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)READ_RESULTS & 0xF) << 28);

    return instruction;
}

//0x5
uint32_t encode_set_mode(int add_sel)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)SET_MODE & 0xF) << 28);
    instruction |= ((uint32_t)add_sel & 0x1);

    return instruction;
}

//0x6
uint32_t encode_pipeline_instr(int opcode, int dst_reg, int src_reg, int src2_reg, int unit_id)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)opcode & 0xF) << 13);
    instruction |= (((uint32_t)dst_reg & 0x7) << 10);
    instruction |= (((uint32_t)src_reg & 0x7) << 7);
    instruction |= (((uint32_t)src2_reg & 0x7) << 4);
    instruction |= ((uint32_t)unit_id & 0xF);

    return instruction & 0x1FFFF;
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

    instruction |= (((uint32_t)LOAD_INSTR & 0xF) << 28);
    instruction |= (((uint32_t)address & 0x3F) << 17);
    instruction |= (pipeline & 0x1FFFF);
    
    return instruction;
}

//0x7
uint32_t encode_run_program(int prog_len)
{
    uint32_t instruction = 0;

    instruction |= ((uint32_t)RUN_PROGRAM << 28);
    instruction |= (uint32_t)prog_len;

    return instruction;
}

//0x8
uint32_t encode_set_rescale(int shift)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)SET_RESCALE & 0xF) << 28);
    instruction |= (((uint32_t)shift & 0x1F) & 0x1F);

    return instruction;
}

//0x9
uint32_t encode_load_ssm_coef(int coef_selector, int channel, int coefficient)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)LOAD_SSM_COEF & 0xF) << 28);
    instruction |= (((uint32_t)coef_selector & 0x3) << 26);
    instruction |= (((uint32_t)channel & 0x1F) << 21);
    instruction |= ((uint32_t)coefficient & 0xFFFF);

    return instruction;
}

//0xA
uint32_t encode_clear_ssm_state(void)
{
    uint32_t instruction = 0;
    
    instruction |= (((uint32_t)CLEAR_SSM_STATE & 0xF) << 28);
    
    return instruction;
}

//0xB
uint32_t encode_set_rope_pos(int position)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)SET_ROPE_POS & 0xF) << 28);
    instruction |= ((uint32_t)position & 0x1F);
    
    return instruction;
}

//0xC
uint32_t encode_load_bn_param(int param_selector, int channel, int param)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)LOAD_BN_PARAM & 0xF) << 28);
    instruction |= (((uint32_t)param_selector & 0x3) << 26);
    instruction |= (((uint32_t)channel & 0x1F) << 21);
    instruction |= ((uint32_t)param & 0xFFFF);

    return instruction;
}

//0xD
uint32_t encode_set_quant_params(int scale, int shift, int zero_point)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)SET_QUANT_PARAMS & 0xF) << 28);
    instruction |= (((uint32_t)scale & 0x7FFF) << 13);
    instruction |= (((uint32_t)shift & 0x1F) << 8);
    instruction |= ((uint32_t)zero_point & 0xFF);
    
    return instruction;
}

//0xE
uint32_t encode_load_weight_burst(int address, int weight)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)LOAD_WEIGHT_BURST & 0xF) << 28);
    instruction |= (((uint32_t)address & 0x3FF) << 8); 
    instruction |= ((uint32_t)weight & 0xFF);

    return instruction;
}

//0xF
uint32_t encode_flush_weights(void)
{
    uint32_t instruction = 0;

    instruction |= (((uint32_t)FLUSH_WEIGHTS & 0xF) << 28);
    
    return instruction;
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
        long value = strtol(instruction->args[i], &end, 0);

        if(*end != '\0')
        {
            args[i] = 0;
        }else
        {
            args[i] = (int)value;
        }
    }

    if(!strcmp(instruction->name, "LOAD_WEIGHT"))
    {
        *encoded = encode_load_weight
                (
                    args[0],
                    args[1],
                    args[2]
                );
        return 1;
    }

    if(!strcmp(instruction->name, "LOAD_ACT"))
    {
        *encoded = encode_load_act
                (
                    args[0],
                    args[1]
                );

        return 1;
    }

    if(!strcmp(instruction->name, "RUN"))
    {
        *encoded = encode_run
                (
                    args[0]
                );

        return 1;
    }

    if(!strcmp(instruction->name, "READ_RESULTS"))
    {
        *encoded = encode_read_results();
        return 1;
    }

    if(!strcmp(instruction->name, "SET_MODE"))
    {
        *encoded = encode_set_mode
                (
                    args[0]
                );
        return 1;
    }

    //0x6
    if(!strcmp(instruction->name, "LOAD_INSTR"))
    {
        int address = args[0];
        int opcode = parse_pipeline(instruction->args[1]);
        int dst = 0, src = 0, src2 = 0, unit_id = 0;
        if(opcode == -1)
        {
            return 0;
        }

        if(opcode == PIPE_NOP || opcode == PIPE_HALT)
        {
            // return 1;
        }else if(opcode == PIPE_STORE)
        {
            dst = parse_register(instruction->args[2]);
            src = parse_register(instruction->args[3]);
        }else if(opcode == PIPE_ADD)
        {
            dst = parse_register(instruction->args[2]);
            src = parse_register(instruction->args[3]);
            src2 = parse_register(instruction->args[4]);
        }else if(opcode == PIPE_COMPUTE)
        {
            if(instruction->arg_count == 5)
            {
                dst = parse_register(instruction->args[2]);
                src = parse_register(instruction->args[3]);
                unit_id = parse_unit_id(instruction->args[4]);
            }else if(instruction->arg_count == 6)
            {
                dst = parse_register(instruction->args[2]);
                src = parse_register(instruction->args[3]);
                src2 = parse_register(instruction->args[4]);
                unit_id = parse_unit_id(instruction->args[5]);
            }else
            {
                return 0;
            }
        }
        *encoded = encode_load_instr
                (
                    address,
                    opcode,
                    dst,
                    src,
                    src2,
                    unit_id
                );
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

    //0x9
    if(strcmp(instruction->name, "LOAD_SSM_COEF") == 0)
    {
        int coef_selector = parse_coef_selector(instruction->args[0] + 6);
        int channel = (int)strtol(instruction->args[1] + 3, NULL, 0);
        int coefficient = (int)strtol(instruction->args[2], NULL, 0);

        *encoded = encode_load_ssm_coef
                (
                    coef_selector,
                    channel,
                    coefficient
                );
        return 1;
    }

    //0xA
    if(strcmp(instruction->name, "CLEAR_SSM_STATE") == 0)
    {
        *encoded = encode_clear_ssm_state();
        return 1;
    }

    //0xB
    if(strcmp(instruction->name, "SET_ROPE_POS") == 0)
    {
        *encoded = encode_set_rope_pos
                (
                    args[0]
                );
        return 1;
    }

    //0xC
    if(strcmp(instruction->name, "LOAD_BN_PARAM") == 0)
    {
        int param_selector = parse_param_selector(instruction->args[0] + 6);
        int channel = (int)strtol(instruction->args[1] + 3, NULL, 0);
        int param = (int)strtol(instruction->args[2], NULL, 0);

        *encoded = encode_load_bn_param
                (
                    param_selector,
                    channel,
                    param
                );

        return 1;
    }

    //0xD
    if(strcmp(instruction->name, "SET_QUANT_PARAMS") == 0)
    {
        *encoded = encode_set_quant_params
                (
                    args[0],
                    args[1],
                    args[2]
                );
        return 1;
    }

    //0xE
    if(strcmp(instruction->name, "LOAD_WEIGHT_BURST") == 0)
    {
        *encoded = encode_load_weight_burst
                (
                    args[0],
                    args[1]
                );
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