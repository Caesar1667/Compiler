#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "opparser.h"
#include "validator.h"
#include "enconder.h"

int main(int argc, char *argv[])
{
    printf("COMPUTE = %d\n", parse_pipeline("COMPUTE"));
    printf("R4      = %d\n", parse_register("R4"));
    printf("RMSNORM = %d\n", parse_unit_id("RMSNORM"));

    return 0;
}