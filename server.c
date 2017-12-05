#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "configurer.h"
#include "dirmanager.h"
#include "mm.h"
#include "syncprot.h"
#include "xsocket.h"

typedef struct
{
    SOCKET s;
} Listener_t;

typedef struct
{
    SOCKET clientSocket;
    struct sockaddr_in clientInfo;
} ServingData_t;

static int _CreateWorkingFolder(SynchronizationServer_t *server);
static int _CreateListener(SynchronizationServer_t *server, Listener_t *listenerInstance);
static void _StartAcceptClients(Listener_t *listenerInstance);
static void *_ServingThreadEntry(void *arg);
static int _HandleIncomingClient(Listener_t *listenerInstance);

void *ServerThreadEntry(void *arg)
{
    SynchronizationServer_t *server = (SynchronizationServer_t *)arg;
    Listener_t l;

    SyncProtWaitForInitialization(server->sp);
    if (_CreateWorkingFolder(server))
        pthread_exit(NULL);

    if (_CreateListener(server, &l))
        pthread_exit(NULL);

    _StartAcceptClients(&l);
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

static int _CreateListener(SynchronizationServer_t *server, Listener_t *listenerInstance)
{
    struct sockaddr_in serverInfo;
    SOCKET s;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        return 1;

    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(server->listeningPort);
    if (bind(s, (struct sockaddr *)&serverInfo, sizeof(serverInfo)))
    {
        socketClose(s);
        return 1;
    }

    if (listen(s, SOMAXCONN))
    {
        socketClose(s);
        return 1;
    }

    listenerInstance->s = s;
    return 0;
}

static void _StartAcceptClients(Listener_t *listenerInstance)
{
    fd_set fset;
    struct timeval tv;
    SOCKET s = listenerInstance->s;
    int r;

    FD_ZERO(&fset);
    FD_SET(s, &fset);
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = 1;

    while ((r = select(s + 1, &fset, NULL, NULL, &tv)) >= 0)
    {
        if (r != 0)
            _HandleIncomingClient(listenerInstance);

        FD_ZERO(&fset);
        FD_SET(s, &fset);
        memset(&tv, 0, sizeof(tv));
        tv.tv_sec = 1;
        pthread_testcancel();
    }
}

static int _HandleIncomingClient(Listener_t *listenerInstance)
{
    struct sockaddr_in clientInfo;
    ServingData_t *sd;
    pthread_t servingThread;
    SOCKET s = listenerInstance->s, c;
    int addrlen;

    addrlen = sizeof(clientInfo);
    c = accept(s, (struct sockaddr *)&clientInfo, &addrlen);
    if (c != INVALID_SOCKET)
    {
        sd = (ServingData_t *)Mmalloc(sizeof(*sd));
        sd->clientSocket = c;
        memcpy(&(sd->clientInfo), &clientInfo, sizeof(clientInfo));
        if (!pthread_create(&servingThread, NULL, _ServingThreadEntry, sd))
        {
            if (pthread_detach(servingThread))
            {
                abort();
                return 1;
            }
        }
        else
        {
            Mfree(sd);
            return 1;
        }
    }
    return 0;
}

static void *_ServingThreadEntry(void *arg)
{
    ServingData_t *sd = (ServingData_t *)arg;

    socketClose(sd->clientSocket);
    Mfree(sd);
    return NULL;
}
