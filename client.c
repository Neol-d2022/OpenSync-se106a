#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "configurer.h"
#include "dirmanager.h"
#include "netwprot.h"
#include "syncprot.h"
#include "xsocket.h"

typedef struct
{
    SOCKET serverSocket;
    struct sockaddr_in serverInfo;
} ConnectionToServer_t;

static int _CreateWorkingFolder(SynchronizationClient_t *client);
static int _CreateConnection(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static int _ClientProtocol(SynchronizationClient_t *client, ConnectionToServer_t *conn);
static int _ClientProtocolHandshake(SynchronizationClient_t *client, ConnectionToServer_t *conn);

void *ClientThreadEntry(void *arg)
{
    ConnectionToServer_t conn;
    SynchronizationClient_t *client = (SynchronizationClient_t *)arg;

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
    return 0;
}

static int _ClientProtocol(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    if (_ClientProtocolHandshake(client, conn))
        return 1;
    return 0;
}

static int _ClientProtocolHandshake(SynchronizationClient_t *client, ConnectionToServer_t *conn)
{
    SocketMessage_t sm;
    struct timeval tv;
    int r;
    uint32_t mn = (uint32_t)(client->magicNumber);
    unsigned char buf[sizeof(mn)];

    NetwProtUInt32ToBuf(buf, mn);
    sm.messageType = NETWPROT_SM_MESSAGE_TYPE_HANDSHAKE;
    sm.messageLength = sizeof(buf);
    sm.message = buf;

    r = NetwProtSendTo(conn->serverSocket, &sm);
    if (r)
        return 1;

    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = NETWPROT_READ_TIMEOUT_IN_SECOND;
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
