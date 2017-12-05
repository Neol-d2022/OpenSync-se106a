#ifndef _SYNCPROT_H_LOADED
#define _SYNCPROT_H_LOADED

#include <pthread.h>

typedef struct
{
    pthread_rwlock_t startupLock;
} SynchronizationProtocols_t;

void SyncProtBeforeInitialization(SynchronizationProtocols_t *sp);
void SyncProtAfterInitialization(SynchronizationProtocols_t *sp);
void SyncProtWaitForInitialization(SynchronizationProtocols_t *sp);
int SyncProtSetCancelable(void);

#endif
