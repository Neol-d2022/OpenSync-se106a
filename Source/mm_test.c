#include <stdio.h>

#include "mm.h"

static const size_t _arrayTestSizes1[] = {4, 8, 16, 256, 1024, 16384};
static const size_t _arrayTestSizes2[] = {8, 9, 72, 512, 1768, 98765};
#define _numTestSizes (sizeof(_arrayTestSizes1) / sizeof(_arrayTestSizes1[0]))

int mm_test(void)
{
    void *p[_numTestSizes];
    size_t i, n;

    printf("Testing Mmalloc()\n");
    for (i = 0; i < _numTestSizes; i += 1)
    {
        printf("T1-%u:\tRequesting %5uB of memory...", (unsigned int)i, (unsigned int)_arrayTestSizes1[i]);
        p[i] = Mmalloc(_arrayTestSizes1[i]);
        if (p[i])
            printf("%p returned, PASSED\n", p[i]);
        else
        {
            printf("NULL returned. TEST FAILED.\n");
            return 1;
        }
    }

    printf("Testing MDebug()\n");
    n = MDebug();
    printf("T2:\tExpected value %u, Actual value %u, ", (unsigned int)_numTestSizes, (unsigned int)n);
    if (n == _numTestSizes)
        printf("PASSED.\n");
    else
    {
        printf("TEST FAILED.\n");
        return 1;
    }

    printf("Testing Mmalloc()\n");
    for (i = 0; i < _numTestSizes; i += 1)
    {
        printf("T3-%u:\tRelocating memory %5uB to %5uB...", (unsigned int)i, (unsigned int)_arrayTestSizes1[i], (unsigned int)_arrayTestSizes2[i]);
        p[i] = Mrealloc(p[i], _arrayTestSizes2[i]);
        if (p[i])
            printf("%p returned, PASSED\n", p[i]);
        else
        {
            printf("NULL returned. TEST FAILED.\n");
            return 1;
        }
    }

    printf("Testing MDebug()\n");
    n = MDebug();
    printf("T4:\tExpected value %u, Actual value %u, ", (unsigned int)_numTestSizes, (unsigned int)n);
    if (n == _numTestSizes)
        printf("PASSED.\n");
    else
    {
        printf("TEST FAILED.\n");
        return 1;
    }

    printf("Testing Mfree()...");
    for (i = 0; i < _numTestSizes; i += 1)
        Mfree(p[i]);
    printf("No results returned\n");

    printf("Testing MDebug()\n");
    n = MDebug();
    printf("T5:\tExpected value 0, Actual value %u, ", (unsigned int)n);
    if (n == 0)
        printf("PASSED.\n");
    else
    {
        printf("TEST FAILED.\n");
        return 1;
    }

    return 0;
}
