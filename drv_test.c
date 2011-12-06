#include <stdio.h>

#include "drv_test_hwd.h"

int
main()
{
    unsigned int v;

    v = TEST_REG1_RMKS(HI_FULL, LO_SOGGY);

    printf("REG1(FULL, SOGGY): %#x\n", v);

    v = TEST_REG1_DEFAULT;

    printf("REG1_DEFAULT: %#x\n", v);

    v = TEST_REG1_RMKS(HI_VAL(6), LO_OF(4));

    printf("REG1(VAL(6), OF(4)): %#x\n", v);

    return 0;
}

