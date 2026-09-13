#include <stdio.h>
#include <string.h>
#include "opparser.h"

int parse_instruction(const char *line, ParsedInstruction *instruction)
{
    char buffer[256];
    char *open_paren;
    char *close_paren;
    char *token;

    if(line == NULL || instruction == NULL)
    {
        return 0;
    }

    memset(instruction, 0, sizeof(ParsedInstruction));

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    open_paren = strchr(buffer, '(');

    if(open_paren == NULL)
    {
        return 0;
    }

    close_paren = strchr(buffer, ')');

    if(close_paren == NULL)
    {
        return 0;
    }

    *open_paren = '\0';
    if(sscanf(buffer, "%31s", instruction->name) != 1)
    {
        return 0;
    }

    *close_paren = '\0';
    token = strtok(open_paren + 1, ",");

    while(token != NULL)
    {
        if(instruction->arg_count >= MAX_ARGS)
        {
            return 0;
        }

        if(sscanf(token, "%d", &instruction->args[instruction->arg_count]) != 1)
        {
            return 0;
        }

        instruction->arg_count++;
        token = strtok(NULL, ",");
    }

    return 1;
}