#include <stdio.h>

#include "configurer.h"
#include "mm.h"

static const char *filenames[] = {
    "testconfig/config1.txt",
    "testconfig/config2.txt",
    "testconfig/config3.txt",
    "testconfig/config4.txt",
    "testconfig/config5.txt",
    "testconfig/config6.txt",
    "testconfig/config7.txt",
    "testconfig/config8.txt",
    "testconfig/config9.txt",
    "testconfig/config10.txt"};

#define NFILENAMES (sizeof(filenames) / sizeof(filenames[0]))

int configurer_test()
{
    Configuration_t c;
    size_t m1, m2;
    size_t i, n;

    n = NFILENAMES;
    for (i = 0; i < n; i += 1)
    {
        m1 = MDebug();
        {
            printf("Reading config file: \"%s\"...\n", filenames[i]);
            ConfigurerReadConfig(filenames[i], &c);
            ConfigurerDebugPrint(&c);
            ConfigurerRelease(&c);
        }
        m2 = MDebug();
        printf("Testing memory leaks...\nExpected = %u, Actual = %u...", (unsigned int)m1, (unsigned int)m2);
        if (m1 == m2)
            printf("PASSED\n");
        else
        {
            printf("TEST FAILED\n");
            return 1;
        }
    }

    return 0;
}
