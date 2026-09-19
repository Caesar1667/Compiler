#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "opparser.h"

static char *trim_whitespace(char *str)
{
    char *end;

    while(isspace((unsigned char)*str))
    {
        str++;
    }
    if(*str == 0)
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end))
    {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

int parse_instruction(const char *line, ParsedInstruction *instruction)
{
    char buffer[256];
    char *comment_pos;
    char *paren_open;
    char *paren_close;
    char *name_token;
    char *arg_token;

    if(line == NULL || instruction == NULL)
    {
        return 0;
    }

    instruction->arg_count = 0;

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    comment_pos = strpbrk(buffer, ";#");
    if(comment_pos != NULL)
    {
        *comment_pos = '\0';
    }

    char *clean_line = trim_whitespace(buffer);
    if(clean_line[0] == '\0')
    {
        return 0;
    }

    paren_open = strchr(clean_line, '(');
    if(paren_open == NULL)
    {
        strncpy(instruction->name, clean_line, MAX_INSTRUCTION_NAME - 1);
        instruction->name[MAX_INSTRUCTION_NAME - 1] = '\0';
        return 1;
    }

    *paren_open = '\0';
    name_token = trim_whitespace(clean_line);
    strncpy(instruction->name, name_token, MAX_INSTRUCTION_NAME - 1);
    instruction->name[MAX_INSTRUCTION_NAME - 1] = '\0';

    paren_close = strrchr(paren_open + 1, ')');
    if(paren_close != NULL)
    {
        *paren_close = '\0';
    }

    char *args_str = paren_open + 1;
    if(!strcmp(instruction->name, "LOAD_INSTR"))
    {
        char *first_comma = strchr(args_str, ',');
        if(first_comma == NULL)
        {
            return 0;
        }
        *first_comma = '\0';

        char *addr = trim_whitespace(args_str);
        strncpy(instruction->args[0], addr, MAX_ARG_LENGTH - 1);
        instruction->args[0][MAX_ARG_LENGTH - 1] = '\0';
        instruction->arg_count = 1;

        char *rest = first_comma + 1;
        char *sub_token = strtok(rest, " \t,");
        while(sub_token != NULL && instruction->arg_count < MAX_ARGS)
        {
            char *clean_sub = trim_whitespace(sub_token);
            if(clean_sub[0] != '\0')
            {
                strncpy(instruction->args[instruction->arg_count], clean_sub, MAX_ARG_LENGTH - 1);
                instruction->args[instruction->arg_count][MAX_ARG_LENGTH - 1] = '\0';
                instruction->arg_count++;
            }
            sub_token = strtok(NULL, " \t,");
        }
        return 1;
    }

    arg_token = strtok(paren_open + 1, ",");
    while(arg_token != NULL && instruction->arg_count < MAX_ARGS)
    {
        char *clean_arg = trim_whitespace(arg_token);
        if(clean_arg[0] != '\0')
        {
            strncpy(instruction->args[instruction->arg_count], clean_arg, MAX_ARG_LENGTH - 1);
            instruction->args[instruction->arg_count][MAX_ARG_LENGTH - 1] = '\0';
            instruction->arg_count++;
        }
        arg_token = strtok(NULL, ",");
    }
    return 1;
}