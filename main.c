#include <stdio.h>

#include "mm_test.h"

int selfTest(void)
{
    if (mm_test())
        return 1;
}

int main(void)
{
    printf("Hello, I am OpenSync-se106a. I am currently under construction.\n");
    if (selfTest())
    {
        printf("Self test failed. Exiting now.\n");
        return 1;
    }
    return 0;
}
