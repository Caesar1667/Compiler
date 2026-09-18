#include <stdio.h>
#include <string.h>
#include "opparser.h"

int parse_instruction(const char *line, ParsedInstruction *instruction)
{
    char buffer[256];
    char *name;
    char *args;
    char *token;

    if(line == NULL || instruction == NULL)
    {
        return 0;
    }

    strcpy(buffer, line);
    instruction->arg_count = 0;
    name = strtok(buffer, "(");
    if(name == NULL)
    {
        return 0;
    }

    strcpy(instruction->name, name);
    args = strtok(NULL, ")");
    if(args == NULL)
    {
        return 1;
    }

    token = strtok(args, ",");

    while(token != NULL && instruction->arg_count < MAX_ARGS)
    {
        while(*token == ' ')
        {
            token++;
        }
        strcpy(instruction->args[instruction->arg_count], token);
        instruction->arg_count++;
        token = strtok(NULL, ",");
    }

    return 1;
}