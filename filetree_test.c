#include <stdio.h>
#include <string.h>

#include "filetree.h"
#include "mm.h"

int filetree_test(void)
{
    MemoryBlock_t mb;
    size_t i, j;
    FileTree_t t, *t2;
    FILE *f;
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

    printf("Testing FileTreeComputeCRC32()\n");
    r = FileTreeComputeCRC32(&t);
    printf("T2:\t%d returned...", r);
    if (r == 0)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        FileTreeDeInit(&t);
        return 1;
    }
    FileTreeDebugPrint(&t);

    printf("Testing FileTreeToMemoryblock()\n");
    FileTreeToMemoryblock(&t, &mb);
    printf("T3:\t%p returned, with size %u...", mb.ptr, (unsigned int)mb.size);
    if (mb.ptr && mb.size)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        FileTreeDeInit(&t);
        return 1;
    }
    printf("Writing results to filetree.bin for debugging purposes.\n");
    f = fopen("filetree.bin", "wb");
    if (f)
    {
        fwrite(mb.ptr, mb.size, 1, f);
        fclose(f);
    }

    printf("Testing FileTreeFromMemoryBlock()\n");
    t2 = FileTreeFromMemoryBlock(&mb, t.basePath);
    printf("T4:\t%p returned", t2);
    if (t2)
    {
        printf(", with total files count %u and total folders cout %u...", (unsigned int)t2->totalFilesLen, (unsigned int)t2->totalFoldersLen);
        if (t.totalFilesLen == t2->totalFilesLen && t.totalFoldersLen == t2->totalFoldersLen)
            printf("PASSED\n");
        else
        {
            printf("TEST FAILED\n");
            FileTreeDeInit(&t);
            FileTreeDeInit(t2);
            Mfree(t2);
            MBfree(&mb);
            return 1;
        }
    }
    else
    {
        printf("...TEST FAILED\n");
        FileTreeDeInit(&t);
        MBfree(&mb);
        return 1;
    }

    FileTreeDeInit(&t);
    FileTreeDebugPrint(t2);
    FileTreeDeInit(t2);
    Mfree(t2);

    mb.size -= 1;
    printf("Testing FileTreeFromMemoryBlock() invalid memory block.\n");
    t2 = FileTreeFromMemoryBlock(&mb, t.basePath);
    printf("T5:\t%p returned", t2);
    if (t2)
    {
        printf(", with total files count %u and total folders cout %u...", (unsigned int)t2->totalFilesLen, (unsigned int)t2->totalFoldersLen);
        printf("TEST FAILED\n");
        FileTreeDeInit(t2);
        Mfree(t2);
        MBfree(&mb);
        return 1;
    }
    else
    {
        printf("...PASSED\n");
    }
    MBfree(&mb);

    j = MDebug();
    printf("Testing Memory Leaks.\n");
    printf("T6:\tExpected = %u, Actual = %u...", (unsigned int)i, (unsigned int)j);
    if (i == j)
        printf("PASSED\n");
    else
    {
        printf("TEST FAILED\n");
        return 1;
    }

    return 0;
}
