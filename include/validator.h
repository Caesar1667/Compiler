#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "opparser.h"

int parse_pipeline(const char *token);
int parse_register(const char *token);
int parse_unit_id(const char *token);
int parse_ssm_coef_selector(const char *token);
int validate_instruction(const ParsedInstruction *instruction);

#endif