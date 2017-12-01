#include <dirent.h>
#include <pthread.h>
#include <stdio.h>

#include "configurer.h"
#include "dirmanager.h"
#include "syncprot.h"

static int _CreateWorkingFolder(SynchronizationServer_t *server);

void *ServerThreadEntry(void *arg)
{
    SynchronizationServer_t *server = (SynchronizationServer_t *)arg;

    SyncProtWaitForInitialization(server->sp);
    if (_CreateWorkingFolder(server))
        pthread_exit(NULL);

    pthread_exit(NULL);
    return NULL;
}

static int _CreateWorkingFolder(SynchronizationServer_t *server)
{
    DIR *dir;
    int r;

    dir = opendir(server->workingFolder);
    if (!dir)
    {
        // 0755
        r = mkdir(server->workingFolder, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        return r;
    }

    closedir(dir);
    return 0;
}
