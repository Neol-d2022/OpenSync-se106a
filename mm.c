#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define _NO_MEM_MSG "Program out of memory.\n"
static size_t _gAllocationCount = 0;

void *Mmalloc(size_t s)
{
    void *p;

    p = malloc(s);
    if (!p)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }

    _gAllocationCount += 1;
    return p;
}

void *Mrealloc(void *p, size_t s)
{
    void *q;

    q = realloc(p, s);
    if (!q)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }

    return q;
}

void Mfree(void *p)
{
    free(p);

    if (p)
        _gAllocationCount -= 1;
}

size_t MDebug()
{
    return _gAllocationCount;
}
