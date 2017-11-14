#include <stdio.h>
#include <string.h>

#include "strings.h"
#include "mm.h"

static const char *testDup = "Duplicate test";
#define testCatSize 2
static const char *testCat[testCatSize] = {"Concatenate ", "Test."};
static const char *testCatExpected = "Concatenate Test.";

int strings_test(void)
{
    size_t i = MDebug(), j;
    char *s;

    printf("Testing SDup()\n");
    s = SDup(testDup);
    printf("T1:\tOriginal = %s, Duplicated = %s\n...", testDup, s);
    if (strcmp(testDup, s))
    {
        printf("TEST FAILED\n");
        return 1;
    }
    else
        printf("PASSED\n");
    Mfree(s);

    printf("Testing SConcat()\n");
    s = SConcat(testCat[0], testCat[1]);
    printf("T2:\tExpected = %s, Duplicated = %s\n...", testCatExpected, s);
    if (strcmp(testCatExpected, s))
    {
        printf("TEST FAILED\n");
        return 1;
    }
    else
        printf("PASSED\n");
    Mfree(s);

    printf("Testing SMConcat()\n");
    s = SMConcat(4, "Multiple", " Strings", " Concatenate", " Test.");
    printf("T3:\tExpected = %s, Actual = %s\n...", "Multiple Strings Concatenate Test.", s);
    if (strcmp("Multiple Strings Concatenate Test.", s))
    {
        printf("TEST FAILED\n");
        return 1;
    }
    else
        printf("PASSED\n");
    Mfree(s);

    j = MDebug();
    printf("Testing Memory Leaks.\n");
    printf("T4:\tExpected = %u, Actual = %u...", (unsigned int)i, (unsigned int)j);
    if (i == j)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        return 1;
    }

    return 0;
}
