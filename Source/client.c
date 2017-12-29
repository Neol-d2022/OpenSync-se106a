#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "configurer.h"
#include "dirmanager.h"
#include "filetree.h"
#include "mb.h"
#include "mm.h"
#include "netwprot.h"
#include "strings.h"
#include "syncprot.h"
#include "xsocket.h"

#define _CACHED_OLD_FILETREE_FILENAME "filetree.bin"
#define _FLAG_ISSET(f, x) ((f) & (x))
#define _CLIENT_MAX_ERROR_COUNT 16

typedef struct
{
    SOCKET serverSocket;
    struct sockaddr_in serverInfo;
    uint32_t cachedGeneration;
} ConnectionToServer_t;

static int _CreateWorkingFolder(SynchronizationClient_t *client);
static int _CreateConnection(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static int _ClientProtocol(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static int _ClientProtocolHandshake(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static FileTree_t *_ClientProtocolFileTreeRequest(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static void _SetTimeout(struct timeval *tv, unsigned int seconds);
static int _ClientProtocolUpdateLocalChange(ConnectionToServer_t *conn, const char *filename, const char *syncdir);
static int _ClientProtocolNotifyFileDeleted(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath);
static int _ClientProtocolNotifyFileCreated(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath);
static int _ClientProtocolSyncToServer(SynchronizationClient_t *client, ConnectionToServer_t *conn, const char *filename);
static int _ClientProtocolRequestFile(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath, const char *fileSavePath);
static int _ClientProtocolWorkingLoop(SynchronizationClient_t *client, ConnectionToServer_t *conn, unsigned int *errorCount);
static int _ClientProtocolConnStartUp(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static int _ClientProtocolNotifyFileChanged(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath);
static int _ClientProtocolStartupMerge(SynchronizationClient_t *client, ConnectionToServer_t *conn, const char *filename);
static int _ClientProtocolKeepAlive(ConnectionToServer_t *conn);
static void _ClientProtocolGetDateString(char *datestr, size_t maxSize);

void *ClientThreadEntry(void *arg)
{
    ConnectionToServer_t conn;
    SynchronizationClient_t *client = (SynchronizationClient_t *)arg;

    SyncProtUnsetCancelable();
    SyncProtWaitForInitialization(client->sp);
    if (_CreateWorkingFolder(client))
        pthread_exit(NULL);

    if (_CreateConnection(client, &conn))
        pthread_exit(NULL);

    _ClientProtocol(client, &conn);
    socketClose(conn.serverSocket);
    pthread_exit(NULL);
    return NULL;
}

// ==========================
// Local Function Definitions
// ==========================

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

static int _CreateConnection(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    SOCKET s;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        return 1;

    memset(&(conn->serverInfo), 0, sizeof(conn->serverInfo));
    (conn->serverInfo).sin_family = PF_INET;
    (conn->serverInfo).sin_addr.s_addr = inet_addr(client->remoteIP);
    (conn->serverInfo).sin_port = htons(client->remotePort);
    if ((conn->serverInfo).sin_addr.s_addr == INADDR_NONE)
    {
        socketClose(s);
        return 1;
    }

    if (connect(s, (struct sockaddr *)&(conn->serverInfo), sizeof(conn->serverInfo)))
    {
        socketClose(s);
        return 1;
    }

    conn->serverSocket = s;
    conn->cachedGeneration = 0;
    return 0;
}

static int _ClientProtocol(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    unsigned int errorCount = 0;
    int r;

    if (_ClientProtocolHandshake(client, conn))
        return 1;
    if (_ClientProtocolConnStartUp(client, conn))
        return 1;
    while ((r = _ClientProtocolWorkingLoop(client, conn, &errorCount)) == 0)
        ;
    return r;
}

static int _ClientProtocolHandshake(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    SocketMessage_t sm;
    struct timeval tv;
    int r;
    uint32_t mn = (uint32_t)(client->magicNumber);
    unsigned char buf[sizeof(mn)];

    NetwProtUInt32ToBuf(buf, mn);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_HANDSHAKE, sizeof(buf), buf);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    if (r)
        return 1;

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
    if (r)
        return 1;

    if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
    {
        NetwProtFreeSocketMesg(&sm);
        return 1;
    }

    NetwProtBufToUInt32(sm.message, &mn);
    NetwProtFreeSocketMesg(&sm);
    if (mn != NETWPROT_RESPONSE_OK)
        return 1;

    return 0;
}

static FileTree_t *_ClientProtocolFileTreeRequest(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    SocketMessage_t sm;
    struct timeval tv;
    MemoryBlock_t mb;
    FileTree_t *ft;
    unsigned char *ptr;
    size_t sizeCount;
    uint32_t mn = 0, generation;
    unsigned char buf[sizeof(mn)];
    int r;

    NetwProtUInt32ToBuf(buf, mn);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_FILETREE_REQUEST, sizeof(buf), buf);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    if (r)
        return NULL;

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
    if (r)
        return NULL;

    if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength <= (sizeof(buf) << 1))
    {
        NetwProtFreeSocketMesg(&sm);
        return NULL;
    }

    ptr = sm.message;
    sizeCount = 0;

    NetwProtBufToUInt32(ptr, &mn);
    ptr += sizeof(mn);
    sizeCount += sizeof(mn);
    if (mn != NETWPROT_RESPONSE_OK)
    {
        NetwProtFreeSocketMesg(&sm);
        return NULL;
    }

    NetwProtBufToUInt32(ptr, &generation);
    ptr += sizeof(generation);
    sizeCount += sizeof(generation);

    mb.size = sm.messageLength - sizeCount;
    mb.ptr = ptr;
    ft = FileTreeFromMemoryBlock(&mb, client->basePath);
    if (ft == NULL)
    {
        NetwProtFreeSocketMesg(&sm);
        return NULL;
    }

    NetwProtFreeSocketMesg(&sm);
    conn->cachedGeneration = generation;
    return ft;
}

static void _SetTimeout(struct timeval *tv, unsigned int seconds)
{
    memset(tv, 0, sizeof(*tv));
    tv->tv_sec = seconds;
}

static int _ClientProtocolUpdateLocalChange(ConnectionToServer_t *conn, const char *filename, const char *syncdir)
{
    char datestr[32];
    FileTree_t *nowFT, *fileFT;
    FileNodeDiff_t **diff;
    size_t diffCount, i;
    char *fileFullPathConflict;
    int r;

    fileFT = FileTreeFromFile(filename, syncdir);
    if (fileFT == NULL)
        return 1;

    nowFT = (FileTree_t *)Mmalloc(sizeof(*nowFT));
    FileTreeInit(nowFT);
    FileTreeSetBasePath(nowFT, syncdir);
    r = FileTreeScan(nowFT);
    if (r)
    {
        FileTreeDeInit(nowFT);
        Mfree(nowFT);
        FileTreeDeInit(fileFT);
        Mfree(fileFT);
        return 1;
    }

    r = 0;
    FileTreeComputeCRC32(nowFT);
    FileTreeDiff(fileFT, nowFT, &diff, &diffCount);
    if (diffCount)
    {
        for (i = 0; i < diffCount; i += 1)
        {
            if (diff[i]->from != NULL)
            {
                if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_DELETED))
                {
                    r = _ClientProtocolNotifyFileDeleted(conn, syncdir, diff[i]->from->fullName);
                }
                else if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_MODIFIED))
                {
                    r = _ClientProtocolNotifyFileChanged(conn, syncdir, diff[i]->from->fullName);
                    if (r == 2)
                    {
                        _ClientProtocolGetDateString(datestr, sizeof(datestr));
                        fileFullPathConflict = SConcat(diff[i]->from->fullName, datestr);
                        r = rename(diff[i]->from->fullName, fileFullPathConflict);
                        Mfree(fileFullPathConflict);
                    }
                }
            }
            else if (diff[i]->to != NULL)
            {
                if (FLAG_ISSET(diff[i]->to->flags, FILENODE_FLAG_CREATED))
                {
                    r = _ClientProtocolNotifyFileCreated(conn, syncdir, diff[i]->to->fullName);
                }
            }
            if (r)
                break;
        }
    }

    FileNodeDiffRelease(diff, diffCount);
    FileTreeDeInit(fileFT);
    Mfree(fileFT);

    if (r == 0)
    {
        FileTreeDeInit(nowFT);
        FileTreeInit(nowFT);
        FileTreeSetBasePath(nowFT, syncdir);
        r = FileTreeScan(nowFT);
        if (r)
        {
            FileTreeDeInit(nowFT);
            Mfree(nowFT);
            return 1;
        }
        FileTreeComputeCRC32(nowFT);
        r = FileTreeToFile(filename, nowFT);
    }

    FileTreeDeInit(nowFT);
    Mfree(nowFT);
    return r;
}

static int _ClientProtocolNotifyFileDeleted(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath)
{
    SocketMessage_t sm;
    struct timeval tv;
    MemoryBlock_t mbstr, mbg, out;
    char *serverPath;
    int r;
    uint32_t g = conn->cachedGeneration;
    unsigned char buf[sizeof(g)];

    NetwProtUInt32ToBuf(buf, g);
    mbg.ptr = buf;
    mbg.size = sizeof(buf);
    serverPath = strstr(relativePath, syncdir) + strlen(syncdir);
    MWriteString(&mbstr, serverPath);
    MMConcat(&out, 2, &mbg, &mbstr);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_DELETED, out.size, out.ptr);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    MBfree(&mbstr);
    MBfree(&out);
    if (r)
        return 1;

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
    if (r)
        return 1;

    if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
    {
        NetwProtFreeSocketMesg(&sm);
        return 1;
    }

    NetwProtBufToUInt32(sm.message, &g);
    NetwProtFreeSocketMesg(&sm);
    if (g != NETWPROT_RESPONSE_OK)
        return 1;

    return 0;
}

static int _ClientProtocolNotifyFileCreated(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath)
{
    SocketMessage_t sm;
    struct timeval tv;
    MemoryBlock_t mbstr, mbg, out;
    char *serverPath;
    int r;
    uint32_t g = conn->cachedGeneration;
    unsigned char buf[sizeof(g)];

    NetwProtUInt32ToBuf(buf, g);
    mbg.ptr = buf;
    mbg.size = sizeof(buf);
    serverPath = strstr(relativePath, syncdir) + strlen(syncdir);
    MWriteString(&mbstr, serverPath);
    MMConcat(&out, 2, &mbg, &mbstr);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_CREATED, out.size, out.ptr);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    MBfree(&mbstr);
    MBfree(&out);
    if (r)
        return 1;

    {
        _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
        r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
        if (r)
            return 1;
        if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
        {
            NetwProtFreeSocketMesg(&sm);
            return 1;
        }
        NetwProtBufToUInt32(sm.message, &g);
        NetwProtFreeSocketMesg(&sm);
        if (g != NETWPROT_RESPONSE_OK)
            return 1;
    }

    r = NetwProtSendFile(conn->serverSocket, relativePath);
    if (r)
        return 1;
    else
        return 0;
}

static int _ClientProtocolSyncToServer(SynchronizationClient_t *client, ConnectionToServer_t *conn, const char *filename)
{
    FileTree_t *fileFT, *nowFT, *serverFT;
    FileNodeDiff_t **diff;
    size_t diffCount, i;
    int r;

    fileFT = FileTreeFromFile(filename, client->basePath);
    if (!fileFT)
        return 1;

    nowFT = (FileTree_t *)Mmalloc(sizeof(*nowFT));
    FileTreeInit(nowFT);
    FileTreeSetBasePath(nowFT, client->basePath);
    r = FileTreeScan(nowFT);
    if (r)
    {
        FileTreeDeInit(nowFT);
        Mfree(nowFT);
        FileTreeDeInit(fileFT);
        Mfree(fileFT);
        return 1;
    }

    FileTreeComputeCRC32(nowFT);
    FileTreeDiff(fileFT, nowFT, &diff, &diffCount);
    FileNodeDiffRelease(diff, diffCount);
    FileTreeDeInit(fileFT);
    Mfree(fileFT);
    if (diffCount)
    {
        FileTreeDeInit(nowFT);
        Mfree(nowFT);
        return 1;
    }

    serverFT = _ClientProtocolFileTreeRequest(client, conn);
    if (serverFT == NULL)
    {
        FileTreeDeInit(nowFT);
        Mfree(nowFT);
        return 1;
    }

    r = 0;
    FileTreeComputeCRC32(nowFT);
    FileTreeDiff(nowFT, serverFT, &diff, &diffCount);
    if (diffCount)
    {
        for (i = 0; i < diffCount; i += 1)
        {
            if (diff[i]->from != NULL)
            {
                if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_DELETED))
                {
                    remove(diff[i]->from->fullName);
                }
                else if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_MODIFIED))
                {
                    r = _ClientProtocolRequestFile(conn, client->basePath, diff[i]->to->fullName, diff[i]->to->fullName);
                }
            }
            else if (diff[i]->to != NULL)
            {
                if (FLAG_ISSET(diff[i]->to->flags, FILENODE_FLAG_CREATED))
                {
                    r = _ClientProtocolRequestFile(conn, client->basePath, diff[i]->to->fullName, diff[i]->to->fullName);
                }
            }
            if (r)
                break;
        }
    }
    FileNodeDiffRelease(diff, diffCount);

    FileTreeDeInit(serverFT);
    Mfree(serverFT);
    FileTreeDeInit(nowFT);
    Mfree(nowFT);
    if (r == 0)
    {
        nowFT = (FileTree_t *)Mmalloc(sizeof(*nowFT));
        FileTreeInit(nowFT);
        FileTreeSetBasePath(nowFT, client->basePath);
        r = FileTreeScan(nowFT);
        if (r == 0)
        {
            FileTreeComputeCRC32(nowFT);
            r = FileTreeToFile(filename, nowFT);
        }

        FileTreeDeInit(nowFT);
        Mfree(nowFT);
    }
    return r;
}

static int _ClientProtocolRequestFile(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath, const char *fileSavePath)
{
    SocketMessage_t sm;
    struct timeval tv;
    MemoryBlock_t mbstr, mbg, out;
    char *serverPath;
    int r;
    uint32_t g = conn->cachedGeneration;
    unsigned char buf[sizeof(g)];

    NetwProtUInt32ToBuf(buf, g);
    mbg.ptr = buf;
    mbg.size = sizeof(buf);
    serverPath = strstr(relativePath, syncdir) + strlen(syncdir);
    MWriteString(&mbstr, serverPath);
    MMConcat(&out, 2, &mbg, &mbstr);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_REQUEST_FILE, out.size, out.ptr);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    MBfree(&mbstr);
    MBfree(&out);
    if (r)
        return 1;

    {
        _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
        r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
        if (r)
            return 1;
        if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
        {
            NetwProtFreeSocketMesg(&sm);
            return 1;
        }
        NetwProtBufToUInt32(sm.message, &g);
        NetwProtFreeSocketMesg(&sm);
        if (g != NETWPROT_RESPONSE_OK)
            return 1;
    }

    r = NetwProtRecvFile(conn->serverSocket, fileSavePath, &tv);
    if (r)
        return 1;

    return 0;
}

static int _ClientProtocolWorkingLoop(SynchronizationClient_t *client, ConnectionToServer_t *conn, unsigned int *errorCount)
{
    char *filename = DirManagerPathConcat(client->workingFolder, _CACHED_OLD_FILETREE_FILENAME);

    if (_ClientProtocolKeepAlive(conn))
    {
        Mfree(filename);
        return 1;
    }

    if (_ClientProtocolUpdateLocalChange(conn, filename, client->basePath))
    {
        Mfree(filename);
        return 1;
    }

    if (_ClientProtocolSyncToServer(client, conn, filename))
    {
        *errorCount += 1;
        if (*errorCount > _CLIENT_MAX_ERROR_COUNT)
        {
            Mfree(filename);
            return 1;
        }
    }
    else
        *errorCount = 0;

    Mfree(filename);
    SyncProtSetCancelable();
    pthread_testcancel();
    SyncProtUnsetCancelable();
    sleep(NETWPROT_IDLE_TIMEOUT_CLIENT_IN_SECOND);
    return 0;
}

static int _ClientProtocolConnStartUp(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    struct stat s;
    char *filename;
    int r;

    filename = DirManagerPathConcat(client->workingFolder, _CACHED_OLD_FILETREE_FILENAME);
    r = stat(filename, &s);

    if (r)
    {
        r = _ClientProtocolStartupMerge(client, conn, filename);
        Mfree(filename);
        return r;
    }

    Mfree(filename);
    return 0;
}

static int _ClientProtocolStartupMerge(SynchronizationClient_t *client, ConnectionToServer_t *conn, const char *filename)
{
    FileTree_t *nowFT, *serverFT;
    FileNodeDiff_t **diff;
    size_t diffCount, i;
    int r;

    serverFT = _ClientProtocolFileTreeRequest(client, conn);
    if (serverFT == NULL)
        return 1;

    nowFT = (FileTree_t *)Mmalloc(sizeof(*nowFT));
    FileTreeInit(nowFT);
    FileTreeSetBasePath(nowFT, client->basePath);
    r = FileTreeScan(nowFT);
    if (r)
    {
        FileTreeDeInit(nowFT);
        Mfree(nowFT);
        FileTreeDeInit(serverFT);
        Mfree(serverFT);
        return 1;
    }

    r = 0;
    FileTreeComputeCRC32(nowFT);
    FileTreeDiff(nowFT, serverFT, &diff, &diffCount);
    if (diffCount)
    {
        for (i = 0; i < diffCount; i += 1)
        {
            if (diff[i]->from != NULL)
            {
                if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_DELETED))
                {
                    r = _ClientProtocolNotifyFileCreated(conn, client->basePath, diff[i]->from->fullName);
                }
                else if (FLAG_ISSET(diff[i]->from->flags, FILENODE_FLAG_MODIFIED))
                {
                    r = _ClientProtocolNotifyFileCreated(conn, client->basePath, diff[i]->from->fullName);
                }
            }
            else if (diff[i]->to != NULL)
            {
                if (FLAG_ISSET(diff[i]->to->flags, FILENODE_FLAG_CREATED))
                {
                    r = _ClientProtocolRequestFile(conn, client->basePath, diff[i]->to->fullName, diff[i]->to->fullName);
                }
            }
            if (r)
                break;
        }
    }
    FileNodeDiffRelease(diff, diffCount);
    FileTreeDeInit(serverFT);
    Mfree(serverFT);

    if (r == 0)
    {
        FileTreeDeInit(nowFT);
        FileTreeInit(nowFT);
        FileTreeSetBasePath(nowFT, client->basePath);
        r = FileTreeScan(nowFT);
        if (r)
        {
            FileTreeDeInit(nowFT);
            Mfree(nowFT);
            return 1;
        }
        FileTreeComputeCRC32(nowFT);
        r = FileTreeToFile(filename, nowFT);
    }

    FileTreeDeInit(nowFT);
    Mfree(nowFT);
    return r;
}

static int _ClientProtocolNotifyFileChanged(ConnectionToServer_t *conn, const char *syncdir, const char *relativePath)
{
    SocketMessage_t sm;
    struct timeval tv;
    MemoryBlock_t mbstr, mbg, out;
    char *serverPath;
    int r;
    uint32_t g = conn->cachedGeneration;
    unsigned char buf[sizeof(g)];

    NetwProtUInt32ToBuf(buf, g);
    mbg.ptr = buf;
    mbg.size = sizeof(buf);
    serverPath = strstr(relativePath, syncdir) + strlen(syncdir);
    MWriteString(&mbstr, serverPath);
    MMConcat(&out, 2, &mbg, &mbstr);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_CHANGED, out.size, out.ptr);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    MBfree(&mbstr);
    MBfree(&out);
    if (r)
        return 1;

    {
        _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
        r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
        if (r)
            return 1;
        if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
        {
            NetwProtFreeSocketMesg(&sm);
            return 1;
        }
        NetwProtBufToUInt32(sm.message, &g);
        NetwProtFreeSocketMesg(&sm);
        if (g != NETWPROT_RESPONSE_OK)
            return g;
    }

    r = NetwProtSendFile(conn->serverSocket, relativePath);
    if (r)
        return 1;
    else
        return 0;
}

static int _ClientProtocolKeepAlive(ConnectionToServer_t *conn)
{
    SocketMessage_t sm;
    struct timeval tv;
    int r;
    uint32_t mn = 0;
    unsigned char buf[sizeof(mn)];

    NetwProtUInt32ToBuf(buf, mn);
    NetwProtSetSM(&sm, NETWPROT_SM_MESSAGE_TYPE_KEEPALIVE, sizeof(buf), buf);
    r = NetwProtSendTo(conn->serverSocket, &sm);
    if (r)
        return 1;

    _SetTimeout(&tv, NETWPROT_READ_TIMEOUT_IN_SECOND);
    r = NetwProtReadFrom(conn->serverSocket, &sm, &tv);
    if (r)
        return 1;

    if (sm.messageType != NETWPROT_SM_MESSAGE_TYPE_RESPONSE || sm.messageLength != sizeof(buf))
    {
        NetwProtFreeSocketMesg(&sm);
        return 1;
    }

    NetwProtBufToUInt32(sm.message, &mn);
    NetwProtFreeSocketMesg(&sm);
    if (mn != NETWPROT_RESPONSE_OK)
        return 1;

    return 0;
}

static void _ClientProtocolGetDateString(char *datestr, size_t maxSize)
{
    static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

    struct tm t;
    time_t ti = time(NULL);

    pthread_mutex_lock(&_mutex);
    t = *localtime(&ti);
    pthread_mutex_unlock(&_mutex);

    strftime(datestr, maxSize, "%Y%m%d%H%M%S", &t);
}
