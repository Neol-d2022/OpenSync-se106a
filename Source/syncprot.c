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
    pthread_rwlock_unlock(&(sp->startupLock));
}

int SyncProtSetCancelable(void)
{
    int oldState;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldState);
    return oldState;
}

int SyncProtUnsetCancelable(void)
{
    int oldState;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldState);
    return oldState;
}
