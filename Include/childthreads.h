#ifndef _CHILD_THREADS_H_LOADED
#define _CHILD_THREADS_H_LOADED

#include <pthread.h>

typedef struct
{
    size_t nChildren;
    pthread_t *threads;
} ChildThreads_t;

#endif
