#include <dirent.h>
#include <pthread.h>
#include <stdio.h>

#include "configurer.h"
#include "dirmanager.h"
#include "syncprot.h"

static int _CreateWorkingFolder(SynchronizationClient_t *client);

void *ClientThreadEntry(void *arg)
{
    SynchronizationClient_t *client = (SynchronizationClient_t *)arg;

    SyncProtWaitForInitialization(client->sp);
    if (_CreateWorkingFolder(client))
        pthread_exit(NULL);

    pthread_exit(NULL);
    return NULL;
}

static int _CreateWorkingFolder(SynchronizationClient_t *client)
{
    DIR *dir;
    int r;

    dir = opendir(client->workingFolder);
    if (!dir)
    {
        // 0755
        r = mkdir(client->workingFolder, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        return r;
    }

    closedir(dir);
    return 0;
}
