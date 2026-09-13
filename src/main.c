#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "opparser.h"
#include "validator.h"
#include "enconder.h"

int main(int argc, char *argv[])
{
    FILE *file;
    char line[256];
    ParsedInstruction instruction;
    uint32_t encoded;

    if(argc != 2)
    {
        printf("Usage: %s <program.op>\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "r");

    if(file == NULL)
    {
        printf("Error: Could not open the file '%s'\n", argv[1]);
    }

    while(fgets(line, sizeof(line), file) != NULL)
    {
        if(!parse_instruction(line, &instruction))
        {
            printf("Error: Could not parse line: %s", line);
            fclose(file);
            return 1;
        }

        // printf("Instruction: %s\n", instruction.name);

        if(!validate_instruction(&instruction))
        {
            printf("Error: Invalid instruction: %s\n\n", instruction.name);
            // fclose(file);
            // return 1;
            continue;
        }

        if(!encode_instruction(&instruction, &encoded))
        {
            printf("Error: Encoder not implemented for %s\n", instruction.name);
            // fclose(file);
            // return 1;
        }

        printf("Instruction: %s\n", instruction.name);
        printf("Encoded:    0x%08X\n\n", encoded);
        
    }

    fclose(file);
    return 0;
}