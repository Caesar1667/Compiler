#ifndef OPPARSER_H
#define OPPARSER_H

#define MAX_INSTRUCTION_NAME 32
#define MAX_ARGS 6
#define MAX_ARG_LENGTH 32

typedef struct 
{
    char name[MAX_INSTRUCTION_NAME];
    int args[MAX_ARGS][MAX_ARG_LENGTH];
    int arg_count;
} ParsedInstruction;


int parse_instruction(const char *line, ParsedInstruction *instruction);

#endif