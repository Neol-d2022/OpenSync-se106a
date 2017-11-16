#include <stdio.h>

#include "config.h"
#include "mm_test.h"
#include "strings_test.h"
#include "filetree_test.h"

int selfTest(void)
{
    if (mm_test())
        return 1;
    if (strings_test())
        return 1;
    if (filetree_test())
        return 1;
    return 0;
}

int main(void)
{
    printf("Hello, I am %s. I am currently under construction.\n", PACKAGE_STRING);
    if (selfTest())
    {
        printf("Self test failed. Exiting now.\n");
        return 1;
    }
    return 0;
}
