#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "opparser.h"
#include "validator.h"
#include "encoder.h"

void binary_display(uint32_t value)
{
    int i;

    for(i = 31; i >= 0; i--)
    {
        printf("%d", (value >> i) & 1);

        if(i % 4 == 0 && i != 0)
        {
            printf(" ");
        }
    }

    printf("\n");
}

static void write_msb_first(FILE *fp, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)((value >> 24) & 0xFF);
    bytes[1] = (uint8_t)((value >> 16) & 0xFF);
    bytes[2] = (uint8_t)((value >> 8) & 0xFF);
    bytes[3] = (uint8_t)(value & 0xFF);
    fwrite(bytes, 1, 4, fp);
}

int main(int argc, char *argv[])
{
    FILE *file;
    FILE *bin = NULL;
    char line[256];
    ParsedInstruction instruction;
    uint32_t encoded;
    int line_num = 0;

    if(argc < 2 || argc > 3)
    {
        printf("Usage: %s <program.op> [output.bin]\n", argv[0]);
        return 1;
    }

    file = fopen(argv[1], "r");

    if(file == NULL)
    {
        printf("Error: Could not open the file '%s'\n", argv[1]);
    }

    if(argc == 3)
    {
        bin = fopen(argv[2], "wb");
        if(bin == NULL)
        {
            printf("Error : Could not open output binary file '%s'\n", argv[2]);
            fclose(file);
            return 1;
        }
    }

    while(fgets(line, sizeof(line), file) != NULL)
    {
        line_num++;

        if(!parse_instruction(line, &instruction))
        {
            printf("Error: Could not parse line: %s", line);
            fclose(file);
            return 1;
        }

        // printf("Instruction: %s\n", instruction.name);

        if(!validate_instruction(&instruction))
        {
            printf("Error [Line %d]: Invalid instruction: %s\n\n", line_num, instruction.name);
            // fclose(file);
            // return 1;
            continue;
        }

        if(!encode_instruction(&instruction, &encoded))
        {
            printf("Error [Line %d]: Encoder not implemented for %s\n", line_num, instruction.name);
            // fclose(file);
            // return 1;
        }

        printf("Instruction: %s\n", instruction.name);
        printf("Hexadecimal:    0x%08X\n", encoded);
        printf("Binary:    ");
        binary_display(encoded);
        printf("\n");

        if(bin != NULL)
        {
            write_msb_first(bin, encoded);
        }   
    }

    fclose(file);
    if(bin != NULL)
    {
        fclose(bin);
        printf("Binary output successfully written to '%s'\n", argv[2]);
    }
    return 0;
}