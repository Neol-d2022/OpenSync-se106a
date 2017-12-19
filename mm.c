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

    if (pthread_mutex_lock(&_memlock))
        abort();
    p = malloc(s);
    if (!p)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }

    _gAllocationCount += 1;
    if (pthread_mutex_unlock(&_memlock))
        abort();

    return p;
}

void *Mrealloc(void *p, size_t s)
{
    void *q;

    if (pthread_mutex_lock(&_memlock))
        abort();
    q = realloc(p, s);
    if (!q)
    {
        fprintf(stderr, _NO_MEM_MSG);
        abort();
    }
    if (pthread_mutex_unlock(&_memlock))
        abort();

    return q;
}

void Mfree(void *p)
{
    if (pthread_mutex_lock(&_memlock))
        abort();
    free(p);

    if (p)
        _gAllocationCount -= 1;
    if (pthread_mutex_unlock(&_memlock))
        abort();
}

size_t MDebug()
{
    size_t r;
    if (pthread_mutex_lock(&_memlock))
        abort();
    r = _gAllocationCount;
    if (pthread_mutex_unlock(&_memlock))
        abort();
    return r;
}
