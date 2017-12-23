#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "configurer.h"
#include "dirmanager.h"
#include "filetree.h"
#include "mm.h"
#include "netwprot.h"
#include "syncprot.h"
#include "xsocket.h"

#define _ACCEPT_CLIENT_INTERVAL_IN_SECOND 4
#define _GENERATION_FILENAME ".gen"

typedef struct
{
    FileTree_t ft;
    pthread_rwlock_t svrRwLock;
    pthread_rwlock_t runningLock;
    uint32_t generation;
    volatile int stopSync;

    SOCKET s;
    SynchronizationServer_t *server;
} Listener_t;

typedef struct
{
    SOCKET clientSocket;
    struct sockaddr_in clientInfo;

    SynchronizationServer_t *server;
    FileTree_t *ft;
    uint32_t *generation;
    pthread_rwlock_t *svrRwLock;
    pthread_rwlock_t *runningLock;
    volatile const int *stopping;
} ServingData_t;

static int _CreateWorkingFolder(SynchronizationServer_t *server);
static int _CreateListener(SynchronizationServer_t *server, Listener_t *listenerInstance);
static void _ClearUpListener(void *arg);
static void _StartAcceptClients(Listener_t *listenerInstance);
static void *_ServingThreadEntry(void *arg);
static int _HandleIncomingClient(Listener_t *listenerInstance);
static int _ServerProtocol(ServingData_t *sd);
static int _ServerProtocolHandshake(ServingData_t *sd);
static int _ServerProtocolWaitForRequest(ServingData_t *sd);
static int _CreateListener_SetUpSocketAndLocks(SynchronizationServer_t *server, Listener_t *listenerInstance, SOCKET s);
static void _StartAcceptClients_ResetBeforeSelect(struct timeval *tv, fd_set *fset, SOCKET s);
static void _SetTimeout(struct timeval *tv, unsigned int seconds);
static void _ReadGeneration(const char *filename, uint32_t *generation);
static int _WriteGeneration(const char *filename, const uint32_t *generation);

static int _ServerProtocolRequestHandler_KeepAlive(void **args);
static int _ServerProtocolRequestHandler_FileTree(void **args);

typedef int (*_ServerProtocolRequestHandler_t)(void **args);
static _ServerProtocolRequestHandler_t _requestHandler[NETWPROT_SM_MESSAGE_TYPE_MAX] = {
    NULL,                                    // NULL
    NULL,                                    // NETWPROT_SM_MESSAGE_TYPE_RESPONSE
    NULL,                                    // NETWPROT_SM_MESSAGE_TYPE_HANDSHAKE
    _ServerProtocolRequestHandler_KeepAlive, // NETWPROT_SM_MESSAGE_TYPE_KEEPALIVE
    _ServerProtocolRequestHandler_FileTree,  // NETWPROT_SM_MESSAGE_TYPE_FILETREE_REQUEST
};

void *ServerThreadEntry(void *arg)
{
    SynchronizationServer_t *server = (SynchronizationServer_t *)arg;
    Listener_t l;

    SyncProtUnsetCancelable();
    SyncProtWaitForInitialization(server->sp);
    if (_CreateWorkingFolder(server))
        pthread_exit(NULL);

    if (_CreateListener(server, &l))
        pthread_exit(NULL);

    pthread_cleanup_push(_ClearUpListener, &l);
    _StartAcceptClients(&l);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
    return NULL;
}

// ==========================
// Local Function Definitions
// ==========================

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
    char *generationFilename;
    SOCKET s;
    int r;

    FileTreeInit(&(listenerInstance->ft));
    FileTreeSetBasePath(&(listenerInstance->ft), server->basePath);
    if (FileTreeScan(&(listenerInstance->ft)))
    {
        FileTreeDeInit(&(listenerInstance->ft));
        return 1;
    }

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
    {
        FileTreeDeInit(&(listenerInstance->ft));
        return 1;
    }

    if (_CreateListener_SetUpSocketAndLocks(server, listenerInstance, s))
    {
        FileTreeDeInit(&(listenerInstance->ft));
        socketClose(s);
        return 1;
    }

    listenerInstance->s = s;
    listenerInstance->server = server;
    listenerInstance->stopSync = 0;
    generationFilename = DirManagerPathConcat(server->workingFolder, _GENERATION_FILENAME);
    _ReadGeneration(generationFilename, &(listenerInstance->generation));
    r = _WriteGeneration(generationFilename, &(listenerInstance->generation));
    Mfree(generationFilename);

    return r;
}

static void _ClearUpListener(void *arg)
{
    Listener_t *listenerInstance = (Listener_t *)arg;
    char *generationFilename = DirManagerPathConcat(listenerInstance->server->workingFolder, _GENERATION_FILENAME);

    listenerInstance->stopSync = 1;
    pthread_rwlock_wrlock(&(listenerInstance->runningLock));
    socketClose(listenerInstance->s);
    FileTreeDeInit(&(listenerInstance->ft));
    pthread_rwlock_destroy(&(listenerInstance->svrRwLock));
    pthread_rwlock_unlock(&(listenerInstance->runningLock));
    pthread_rwlock_destroy(&(listenerInstance->runningLock));
    _WriteGeneration(generationFilename, &listenerInstance->generation);
    Mfree(generationFilename);
}

static void _StartAcceptClients(Listener_t *listenerInstance)
{
    fd_set fset;
    struct timeval tv;
    SOCKET s = listenerInstance->s;
    int r;

    _StartAcceptClients_ResetBeforeSelect(&tv, &fset, s);
    while ((r = select(s + 1, &fset, NULL, NULL, &tv)) >= 0)
    {
        if (r != 0)
            _HandleIncomingClient(listenerInstance);

        SyncProtSetCancelable();
        pthread_testcancel();
        SyncProtUnsetCancelable();
        _StartAcceptClients_ResetBeforeSelect(&tv, &fset, s);
    }
}

static int _HandleIncomingClient(Listener_t *listenerInstance)
{
    struct sockaddr_in clientInfo;
    ServingData_t *sd;
    pthread_t servingThread;
    SOCKET s = listenerInstance->s, c;
    socklen_t addrlen;

    addrlen = sizeof(clientInfo);
    c = accept(s, (struct sockaddr *)&clientInfo, &addrlen);
    if (c != INVALID_SOCKET)
    {
        sd = (ServingData_t *)Mmalloc(sizeof(*sd));
        sd->clientSocket = c;
        sd->server = listenerInstance->server;
        sd->ft = &(listenerInstance->ft);
        sd->svrRwLock = &(listenerInstance->svrRwLock);
        sd->runningLock = &(listenerInstance->runningLock);
        sd->stopping = &(listenerInstance->stopSync);
        sd->generation = &(listenerInstance->generation);
        memcpy(&(sd->clientInfo), &clientInfo, sizeof(clientInfo));
        if (!pthread_create(&servingThread, NULL, _ServingThreadEntry, sd))
        {
            if (pthread_detach(servingThread))
                abort();
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
    int oldstate;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    pthread_rwlock_rdlock(sd->runningLock);
    _ServerProtocol(sd);
    pthread_rwlock_unlock(sd->runningLock);
    socketClose(sd->clientSocket);
    Mfree(sd);
    return NULL;
}

static int _ServerProtocol(ServingData_t *sd)
{
    int r;
    if (_ServerProtocolHandshake(sd))
        return 1;
    while ((r = _ServerProtocolWaitForRequest(sd)) == 0)
        ;
    return r;
}

static int _ServerProtocolHandshake(ServingData_t *sd)
{
    SocketMessage_t sm;
    struct timeval tv;
    int r, s;
    uint32_t mn;
    unsigned char buf[sizeof(mn)];

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(sd->clientSocket, &sm, &tv);
    if (r)
        return 1;

    if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_HANDSHAKE || sm.messageLength != sizeof(buf))
    {
        NetwProtFreeSocketMesg(&sm);
        return 1;
    }

    NetwProtBufToUInt32(sm.message, &mn);
    NetwProtFreeSocketMesg(&sm);

    if (mn != (uint32_t)(sd->server->magicNumber))
        r = mn = 1;
    else
        r = mn = 0;

    NetwProtUInt32ToBuf(buf, mn);
    sm.messageType = NETWPROT_SM_MESSAGE_TYPE_RESPONSE;
    sm.messageLength = sizeof(buf);
    sm.message = buf;
    s = NetwProtSendTo(sd->clientSocket, &sm);

    return (r | s) ? 1 : 0;
}

static int _ServerProtocolWaitForRequest(ServingData_t *sd)
{
    SocketMessage_t sm;
    struct timeval tv;
    void *args[2];
    int r, s;

    if (*(sd->stopping) != 0)
        return 1;

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(sd->clientSocket, &sm, &tv);
    if (r)
        return 1;

    if (sm.messageType >= NETWPROT_SM_MESSAGE_TYPE_MAX)
        s = 1;
    else if (_requestHandler[sm.messageType] == NULL)
        s = 1;
    else
    {
        args[0] = sd;
        args[1] = &sm;
        s = (_requestHandler[sm.messageType])(args);
    }

    NetwProtFreeSocketMesg(&sm);
    return (r | s) ? 1 : 0;
}

static int _ServerProtocolRequestHandler_KeepAlive(void **args)
{
    SocketMessage_t sm;
    ServingData_t *sd = args[0];
    //SocketMessage_t *sm = args[1];
    uint32_t mn = 0;
    unsigned char buf[sizeof(mn)];

    NetwProtUInt32ToBuf(buf, mn);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_RESPONSE, sizeof(buf), buf);

    return NetwProtSendTo(sd->clientSocket, &sm);
}

static int _ServerProtocolRequestHandler_FileTree(void **args)
{

    SocketMessage_t sm;
    MemoryBlock_t mb, bufmb, bufgmb, out;
    ServingData_t *sd = args[0];
    //SocketMessage_t *sm = args[1];
    uint32_t mn = 0;
    unsigned char buf[sizeof(mn)];
    unsigned char bufg[sizeof(*(sd->generation))];
    int r;

    pthread_rwlock_rdlock(sd->svrRwLock);
    FileTreeToMemoryblock(sd->ft, &mb);
    NetwProtUInt32ToBuf(bufg, *(sd->generation));
    pthread_rwlock_unlock(sd->svrRwLock);

    NetwProtUInt32ToBuf(buf, mn);
    bufmb.size = sizeof(buf);
    bufmb.ptr = buf;
    bufgmb.size = sizeof(bufg);
    bufgmb.ptr = bufg;
    MMConcat(&out, 3, &bufmb, &bufgmb, &mb);
    MBfree(&mb);

    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_RESPONSE, out.size, out.ptr);
    r = NetwProtSendTo(sd->clientSocket, &sm);

    MBfree(&out);
    return r;
}

static int _CreateListener_SetUpSocketAndLocks(SynchronizationServer_t *server, Listener_t *listenerInstance, SOCKET s)
{
    struct sockaddr_in serverInfo;

    memset(&serverInfo, 0, sizeof(serverInfo));
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(server->listeningPort);

    if (bind(s, (struct sockaddr *)&serverInfo, sizeof(serverInfo)))
        return 1;
    if (listen(s, SOMAXCONN))
        return 1;
    if (pthread_rwlock_init(&(listenerInstance->svrRwLock), NULL))
        return 1;
    if (pthread_rwlock_init(&(listenerInstance->runningLock), NULL))
    {
        pthread_rwlock_destroy(&(listenerInstance->svrRwLock));
        return 1;
    }

    return 0;
}

static void _StartAcceptClients_ResetBeforeSelect(struct timeval *tv, fd_set *fset, SOCKET s)
{
    FD_ZERO(fset);
    FD_SET(s, fset);
    _SetTimeout(tv, _ACCEPT_CLIENT_INTERVAL_IN_SECOND);
}

static void _SetTimeout(struct timeval *tv, unsigned int seconds)
{
    memset(tv, 0, sizeof(*tv));
    tv->tv_sec = seconds;
}

static void _ReadGeneration(const char *filename, uint32_t *generation)
{
    unsigned char *p = (unsigned char *)generation;
    FILE *f;
    size_t n = sizeof(*generation), r;

    f = fopen(filename, "rb");
    if (!f)
    {
        *generation = 0;
        return;
    }

    r = fread(p, n, 1, f);
    fclose(f);

    if (r != 1)
        *generation = 0;

    return;
}

static int _WriteGeneration(const char *filename, const uint32_t *generation)
{
    const unsigned char *p = (const unsigned char *)generation;
    FILE *f;
    size_t n = sizeof(*generation), w;

    f = fopen(filename, "wb");
    if (!f)
        return 1;
    w = fwrite(p, n, 1, f);
    fclose(f);
    return (w == 1) ? 0 : 1;
}
