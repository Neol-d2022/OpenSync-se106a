#include <dirent.h>
#include <pthread.h>
#include <stdio.h>

#include "configurer.h"
#include "dirmanager.h"
#include "syncprot.h"
#include "xsocket.h"

typedef struct
{
    SOCKET serverSocket;
    struct sockaddr_in serverInfo;
} ConnectionToServer_t;

static int _CreateWorkingFolder(SynchronizationClient_t *client);
static int _CreateConnection(SynchronizationClient_t *client, ConnectionToServer_t *conn);

void *ClientThreadEntry(void *arg)
{
    ConnectionToServer_t conn;
    SynchronizationClient_t *client = (SynchronizationClient_t *)arg;

    SyncProtWaitForInitialization(client->sp);
    if (_CreateWorkingFolder(client))
        pthread_exit(NULL);

    if (_CreateConnection(client, &conn))
        pthread_exit(NULL);

    socketClose(conn.serverSocket);
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
