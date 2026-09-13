#include <string.h>
#include "enconder.h"
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
    if(instruction == NULL || encoded == NULL)
    {
        return 0;
    }

    if(strcmp(instruction->name, "LOAD_WEIGHT") == 0)
    {
        *encoded = encode_load_weight
                (
                    instruction->args[0],
                    instruction->args[1],
                    instruction->args[2]
                );
        return 1;
    }

    if(strcmp(instruction->name, "LOAD_ACT") == 0)
    {
        *encoded = encode_load_act
                (
                    instruction->args[0],
                    instruction->args[1]
                );

        return 1;
    }

    if(strcmp(instruction->name, "RUN") == 0)
    {
        *encoded = encode_run
                (
                    instruction->args[0]
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
                    instruction->args[0]
                );
        return 1;
    }

    //0x6

    //0x7
    if(strcmp(instruction->name, "RUN_PROGRAM") == 0)
    {
        *encoded = encode_run_program
                (
                    instruction->args[0]
                );
        return 1;
    }

    //0x8
    if(strcmp(instruction->name, "SET_RESCALE") == 0)
    {
        *encoded = encode_set_rescale
                (
                    instruction->args[0]
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