#include "unistd.h"

#include "syncprot.h"

void SyncProtBeforeInitialization(SynchronizationProtocols_t *sp)
{
    pthread_rwlock_init(&(sp->startupLock), NULL);
    pthread_rwlock_wrlock(&(sp->startupLock));
}

void SyncProtAfterInitialization(SynchronizationProtocols_t *sp)
{
    pthread_rwlock_unlock(&(sp->startupLock));
    sleep(1);
    pthread_rwlock_destroy(&(sp->startupLock));
}

void SyncProtWaitForInitialization(SynchronizationProtocols_t *sp)
{
    pthread_rwlock_rdlock(&(sp->startupLock));
}
