#include <stdio.h>
#include <string.h>

#include "filetree.h"
#include "mm.h"

int filetree_test(void)
{
    size_t i, j;
    FileTree_t t;
    int r;

    i = MDebug();
    printf("Testing FileTreeInit()...No results returned\n");
    FileTreeInit(&t);
    printf("Testing FileTreeSetBasePath()...No results returned\n");
    FileTreeSetBasePath(&t, ".");
    printf("Testing FileTreeScan()\n");
    r = FileTreeScan(&t);
    printf("T1:\t%d returned, with base pointer %p...", r, t.baseChildren);
    if (r == 0 && t.baseChildren)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        FileTreeDeInit(&t);
        return 1;
    }

    FileTreeDebugPrint(&t);
    FileTreeDeInit(&t);

    j = MDebug();
    printf("Testing Memory Leaks.\n");
    printf("T2:\tExpected = %u, Actual = %u...", (unsigned int)i, (unsigned int)j);
    if (i == j)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        return 1;
    }

    return 0;
}
