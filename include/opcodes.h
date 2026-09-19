#ifndef OPCODES_H
#define OPCODES_H

//HOST OPCODES
#define LOAD_WEIGHT         0x1
#define LOAD_ACT            0x2
#define RUN                 0x3
#define READ_RESULTS        0x4
#define SET_MODE            0x5
#define LOAD_INSTR          0x6
#define RUN_PROGRAM         0x7
#define SET_RESCALE         0x8
#define LOAD_SSM_COEF       0x9
#define CLEAR_SSM_STATE     0xA
#define SET_ROPE_POS        0xB
#define LOAD_BN_PARAM       0xC
#define SET_QUANT_PARAMS    0xD
#define LOAD_WEIGHT_BURST   0xE
#define FLUSH_WEIGHTS       0xF

//PIPELINE OPCODES
#define PIPE_NOP            0x0
#define PIPE_COMPUTE        0x1
#define PIPE_STORE          0x2
#define PIPE_ADD            0x3
#define PIPE_HALT           0xF

//Unit ID
#define UNIT_RELU           0
#define UNIT_SIGMOID        1
#define UNIT_SELU           2
#define UNIT_RMSNORM        3
#define UNIT_ZSCORE         4
#define UNIT_SOFTMAX        5
#define UNIT_ROPE           6
#define UNIT_SSM_STEP       7
#define UNIT_BATCHNORM      8
#define UNIT_SWIGLU         9
#define UNIT_QUANTIZE       10
#define UNIT_POOL_MAX       11
#define UNIT_POOL_MIN       12
#define UNIT_POOL_MEAN      13

//Coeffiency Selector
#define COEF_ABAR           0
#define COEF_BBAR           1
#define COEF_C              2
#define COEF_D              3

//Parameter Selector
#define PARAM_MEAN          0
#define PARAM_INV_STD       1
#define PARAM_GAMMA         2
#define PARAM_BETA          3


#define ROWS                32
#define COLS                32
#define DATA_WIDTH          8
#define PIPE_WIDTH          16
#define PROG_DEPTH          64
#define N_POS               32


#endif