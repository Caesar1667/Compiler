#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include "opparser.h"

uint32_t encode_load_weight(int row, int col, int data); //0x1
uint32_t encode_load_act(int row, int data); //0x2
uint32_t encode_run(int clear_acc); //0x3
uint32_t encode_read_results(void); //0x4
uint32_t encode_set_mode(int add_sel); //0x5
uint32_t encode_load_instr(int address, int opcode, int dst_reg, int src_reg, int src2_reg, int unit_id); //0x6
uint32_t encode_pipeline_instr(int opcode, int dst_reg, int src_reg, int src2_reg, int unit_id);
uint32_t encode_run_program(int prog_len);
uint32_t encode_set_rescale(int shift); //0x8
uint32_t encode_load_ssm_coef(int coef_selector, int channel, int coefficient); //0x9
uint32_t encode_clear_ssm_state(void); //0xA
uint32_t encode_set_rope_pos(int position); //0xB
uint32_t encode_load_bn_param(int param_selector, int channel, int param); //0xC
uint32_t encode_set_quant_params(int scale, int shift, int zero_point); //0xD
uint32_t encode_flush_weights(void); //0xF


int encode_instruction(const ParsedInstruction *instruction, uint32_t *encoded);

#endif