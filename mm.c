#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _NO_MEM_MSG "Program out of memory.\n"
static size_t _gAllocationCount = 0;
static pthread_mutex_t _memlock = PTHREAD_MUTEX_INITIALIZER;

void *Mmalloc(size_t s)
{
    void *p;

    pthread_mutex_lock(&_memlock);
    p = malloc(s);
    if (!p)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }

    _gAllocationCount += 1;
    pthread_mutex_unlock(&_memlock);

    return p;
}

void *Mrealloc(void *p, size_t s)
{
    void *q;

    pthread_mutex_lock(&_memlock);
    q = realloc(p, s);
    if (!q)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }
    pthread_mutex_unlock(&_memlock);

    return q;
}

void Mfree(void *p)
{
    pthread_mutex_lock(&_memlock);
    free(p);

    if (p)
        _gAllocationCount -= 1;
    pthread_mutex_unlock(&_memlock);
}

size_t MDebug()
{
    size_t r;
    pthread_mutex_lock(&_memlock);
    r = _gAllocationCount;
    pthread_mutex_unlock(&_memlock);
    return r;
}
