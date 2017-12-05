#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "configurer.h"
#include "mm.h"
#include "server.h"
#include "strings.h"
#include "transformcontainer.h"

#define _CONFIG_FLAG_BASE_PATH_SET 0x00000001
#define _CONFIG_FLAG_REMOTE_IP_SET 0x00000002
#define _CONFIG_FLAG_REMOTE_PORT_SET 0x00000004
#define _CONFIG_FLAG_MAGIC_NUMBER_SET 0x00000008
#define _CONFIG_FLAG_LISTENING_PORT_SET 0x00000010

#define _FLAG_SET(f, x) ((f) |= (x))
#define _FLAG_RESET(f, x) ((f) &= (~(x)))
#define _FLAG_ISSET(f, x) ((f) & (x))

static void _ReleaseClient(SynchronizationClient_t *client);
static void _ReleaseServer(SynchronizationServer_t *server);
static int _Trim_fgets(char *str);
static int _ParseConfig(const char *cfg, char **key, char **value, char **sep);
static void _TrimSpace(const char *str, const char **start, size_t *len);
static int _CheckBoth(TC_t **container, void ***ptr, unsigned int *flags, unsigned int lineCount);
static int _CheckClientAndAdd(TC_t *clients, SynchronizationClient_t *client, unsigned int *flags, unsigned int lineCount);
static int _CheckServerAndAdd(TC_t *servers, SynchronizationServer_t *server, unsigned int *flags, unsigned int lineCount);
static void _CheckFailClearUpClients(void *data, void *param);
static void _CheckFailClearUpServers(void *data, void *param);
static void _ReadConfigString(char **pair, char **dest, unsigned int *flags, int *ret, unsigned int configFlag, unsigned int lineCount);
static void _ReadConfigUInt(char **pair, unsigned int *dest, unsigned int *flags, int *ret, unsigned int configFlag, unsigned int lineCount);
static void _PrintClient(SynchronizationClient_t *client);
static void _PrintServer(SynchronizationServer_t *server);

int ConfigurerReadConfig(const char *filename, Configuration_t *c)
{
    char buf[256];
    TC_t clients, servers;
    TC_t *container[2] = {&clients, &servers};
    char *pair[2], *sep;
    FILE *f;
    SynchronizationClient_t *client = NULL;
    SynchronizationServer_t *server = NULL;
    void **ptr[2] = {(void **)(&client), (void **)(&server)};
    char **basePath = NULL;
    char **remoteIP = NULL;
    unsigned int *remotePort = NULL;
    unsigned int *magicNumber = NULL;
    unsigned int *listeningPort = NULL;
    unsigned int lineCount = 0, flags = 0;
    int ret = 0;

    memset(c, 0, sizeof(*c));
    f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "[Configurer] Cannot open \"%s\"\n", filename);
        return errno;
    }

    TCInit(&clients);
    TCInit(&servers);

    while (fgets(buf, sizeof(buf), f))
    {
        lineCount += 1;

        if (!_Trim_fgets(buf))
        {
            if (fgets(buf, sizeof(buf), f))
            {
                fprintf(stderr, "[Configurer] Line %u: Too much characters on a single line.\n", lineCount);
                ret = -1;
                goto ConfigurerReadConfig_while_end;
            }
            else
            {
                if (ferror(f))
                {
                    fprintf(stderr, "[Configurer] Cannot read \"%s\": IO error.\n", filename);
                    ret = errno;
                    goto ConfigurerReadConfig_while_end;
                }
            }
        }

        if (_ParseConfig(buf, pair + 0, pair + 1, &sep))
        {
            if (!strlen(pair[0]) && !sep) //Skip
            {
            }
            else if (!strcmp(pair[0], "client"))
            {
                if (!_CheckBoth(container, ptr, &flags, lineCount))
                    ret = -1;
                else
                {
                    client = (SynchronizationClient_t *)Mmalloc(sizeof(*client));
                    memset(client, 0, sizeof(*client));
                    basePath = &(client->basePath);
                    remoteIP = &(client->remoteIP);
                    remotePort = &(client->remotePort);
                    magicNumber = &(client->magicNumber);
                    listeningPort = NULL;
                }
            }
            else if (!strcmp(pair[0], "server"))
            {
                if (!_CheckBoth(container, ptr, &flags, lineCount))
                    ret = -1;
                else
                {
                    server = (SynchronizationServer_t *)Mmalloc(sizeof(*server));
                    memset(server, 0, sizeof(*server));
                    basePath = &(server->basePath);
                    remoteIP = NULL;
                    remotePort = NULL;
                    magicNumber = &(server->magicNumber);
                    listeningPort = &(server->listeningPort);
                }
            }
            else if (!strcmp(pair[0], "base_path"))
                _ReadConfigString(pair, basePath, &flags, &ret, _CONFIG_FLAG_BASE_PATH_SET, lineCount);
            else if (!strcmp(pair[0], "remote_ip"))
                _ReadConfigString(pair, remoteIP, &flags, &ret, _CONFIG_FLAG_REMOTE_IP_SET, lineCount);
            else if (!strcmp(pair[0], "remote_port"))
                _ReadConfigUInt(pair, remotePort, &flags, &ret, _CONFIG_FLAG_REMOTE_PORT_SET, lineCount);
            else if (!strcmp(pair[0], "magic_number"))
                _ReadConfigUInt(pair, magicNumber, &flags, &ret, _CONFIG_FLAG_MAGIC_NUMBER_SET, lineCount);
            else if (!strcmp(pair[0], "listening_port"))
                _ReadConfigUInt(pair, listeningPort, &flags, &ret, _CONFIG_FLAG_LISTENING_PORT_SET, lineCount);
            else
            {
                fprintf(stderr, "[Configurer] Line %u: Unregconized Key: \"%s\".\n", lineCount, pair[0]);
                ret = -1;
            }
        }
        else
        {
            fprintf(stderr, "[Configurer] Line %u: Invalid Configuration: \"%s\".\n", lineCount, buf);
            ret = -1;
        }

        Mfree(pair[0]);
        Mfree(pair[1]);

        if (ret)
        {
        ConfigurerReadConfig_while_end:
            TCTravase(&clients, NULL, _CheckFailClearUpClients);
            TCTravase(&servers, NULL, _CheckFailClearUpServers);
            goto ConfigurerReadConfig_exit;
        }
    }

    if (ret == 0)
    {
        if (client)
        {
            if (!_CheckClientAndAdd(&clients, client, &flags, lineCount))
            {
                ret = -1;
                TCTravase(&clients, NULL, _CheckFailClearUpClients);
                TCTravase(&servers, NULL, _CheckFailClearUpServers);
                client = NULL;
                goto ConfigurerReadConfig_exit;
            }
        }

        if (server)
        {
            if (!_CheckServerAndAdd(&servers, server, &flags, lineCount))
            {
                ret = -1;
                TCTravase(&clients, NULL, _CheckFailClearUpClients);
                TCTravase(&servers, NULL, _CheckFailClearUpServers);
                server = NULL;
                goto ConfigurerReadConfig_exit;
            }
        }

        TCTransform(&clients);
        TCTransform(&servers);

        c->nClients = TCCount(&clients);
        c->clients = (SynchronizationClient_t **)Mmalloc(sizeof(*(c->clients)) * c->nClients);
        memcpy(c->clients, clients.fixedStorage.storage, sizeof(*(c->clients)) * c->nClients);
        c->nServers = TCCount(&servers);
        c->servers = (SynchronizationServer_t **)Mmalloc(sizeof(*(c->servers)) * c->nServers);
        memcpy(c->servers, servers.fixedStorage.storage, sizeof(*(c->servers)) * c->nServers);
    }

ConfigurerReadConfig_exit:
    TCDeInit(&clients);
    TCDeInit(&servers);
    fclose(f);
    if (ret != 0)
    {
        if (_FLAG_ISSET(flags, _CONFIG_FLAG_BASE_PATH_SET))
            Mfree(*basePath);
        if (_FLAG_ISSET(flags, _CONFIG_FLAG_REMOTE_IP_SET))
            Mfree(*remoteIP);
        if (client)
            Mfree(client);
        if (server)
            Mfree(server);
    }
    return ret;
}

void ConfigurerRelease(Configuration_t *c)
{
    size_t i;

    if (c->sp)
    {
        size_t i;
        void *ret;
        int r;

        Mfree(c->sp);

        for (i = 0; i < c->nClients; i += 1)
        {
            pthread_cancel((c->clientThreads)[i]);
            r = pthread_join((c->clientThreads)[i], &ret);
            if (r)
                abort();
        }
        Mfree(c->clientThreads);

        for (i = 0; i < c->nServers; i += 1)
        {
            pthread_cancel((c->serverThreads)[i]);
            r = pthread_join((c->serverThreads)[i], &ret);
            if (r)
                abort();
        }
        Mfree(c->serverThreads);
    }

    if (c->clients)
    {
        for (i = 0; i < c->nClients; i += 1)
        {
            _ReleaseClient((c->clients)[i]);
            Mfree((c->clients)[i]);
        }
        Mfree(c->clients);
    }

    if (c->servers)
    {
        for (i = 0; i < c->nServers; i += 1)
        {
            _ReleaseServer((c->servers)[i]);
            Mfree((c->servers)[i]);
        }
        Mfree(c->servers);
    }
}

void ConfigurerDebugPrint(Configuration_t *c)
{
    size_t i;

    printf("Number of clients: %u\n", (unsigned int)(c->nClients));
    for (i = 0; i < c->nClients; i += 1)
        _PrintClient((c->clients)[i]);
    printf("Number of servers: %u\n", (unsigned int)(c->nServers));
    for (i = 0; i < c->nServers; i += 1)
        _PrintServer((c->servers)[i]);
}

int ConfigurerStartup(Configuration_t *c)
{
    char idbuf[16];
    void *ret;
    size_t i, j;
    int r;

    c->sp = (SynchronizationProtocols_t *)Mmalloc(sizeof(*(c->sp)));
    c->clientThreads = (pthread_t *)Mmalloc(sizeof(*(c->clientThreads)) * c->nClients);
    c->serverThreads = (pthread_t *)Mmalloc(sizeof(*(c->serverThreads)) * c->nServers);

    SyncProtBeforeInitialization(c->sp);
    for (i = 0; i < c->nServers; i += 1)
    {
        (c->servers)[i]->sp = c->sp;
        sprintf(idbuf, "%u", (c->servers)[i]->magicNumber);
        (c->servers)[i]->workingFolder = SConcat("Server-", idbuf);
        r = pthread_create(c->serverThreads + i, NULL, ServerThreadEntry, (c->servers)[i]);
        if (r)
        {
            for (j = 0; j < i; j += 1)
            {
                pthread_cancel((c->serverThreads)[j]);
                pthread_join((c->serverThreads)[j], &ret);
                Mfree((c->servers)[j]->workingFolder);
            }
            SyncProtAfterInitialization(c->sp);
            Mfree(c->sp);
            Mfree(c->clientThreads);
            Mfree(c->serverThreads);
            c->sp = NULL;
            c->clientThreads = NULL;
            c->serverThreads = NULL;
            return r;
        }
    }

    for (i = 0; i < c->nClients; i += 1)
    {
        (c->clients)[i]->sp = c->sp;
        sprintf(idbuf, "%u", (c->clients)[i]->magicNumber);
        (c->clients)[i]->workingFolder = SConcat("Client-", idbuf);
        r = pthread_create(c->clientThreads + i, NULL, ClientThreadEntry, (c->clients)[i]);
        if (r)
        {
            for (j = 0; j < i; j += 1)
            {
                pthread_cancel((c->clientThreads)[j]);
                pthread_join((c->clientThreads)[j], &ret);
                Mfree((c->clients)[j]->workingFolder);
            }
            for (j = 0; j < c->nServers; j += 1)
            {
                pthread_cancel((c->serverThreads)[j]);
                pthread_join((c->serverThreads)[j], &ret);
                Mfree((c->servers)[j]->workingFolder);
            }
            SyncProtAfterInitialization(c->sp);
            Mfree(c->sp);
            Mfree(c->clientThreads);
            Mfree(c->serverThreads);
            c->sp = NULL;
            c->clientThreads = NULL;
            c->serverThreads = NULL;
            return r;
        }
    }

    SyncProtAfterInitialization(c->sp);
    return 0;
}

// ==========================
// Local Function Definitions
// ==========================

static void _ReleaseClient(SynchronizationClient_t *client)
{
    Mfree(client->basePath);
    Mfree(client->remoteIP);
    if (client->workingFolder)
        Mfree(client->workingFolder);
}

static void _ReleaseServer(SynchronizationServer_t *server)
{
    Mfree(server->basePath);
    if (server->workingFolder)
        Mfree(server->workingFolder);
}

static int _Trim_fgets(char *str)
{
    char *p;
    int c = 0;

    p = strchr(str, '\n');
    if (p)
    {
        *p = '\0';
        c += 1;
    }

    return c;
}

static int _ParseConfig(const char *cfg, char **key, char **value, char **sep)
{
    static const char SEPERATOR = '=';
    const char *s;
    char *c, *p, *k, *v;
    size_t l;

    p = strchr(cfg, SEPERATOR);
    if (!p)
    {
        *sep = p;
        _TrimSpace(cfg, &s, &l);
        k = (char *)Mmalloc(l + 1);
        if (l)
            memcpy(k, s, l);
        v = (char *)Mmalloc(1);
        v[0] = k[l] = '\0';
    }
    else
    {
        *sep = p;
        c = SDup(cfg);
        p = c + ((size_t)p - (size_t)cfg);
        *p = '\0';

        _TrimSpace(c, &s, &l);
        k = (char *)Mmalloc(l + 1);
        if (l)
            memcpy(k, s, l);
        k[l] = '\0';

        _TrimSpace(p + 1, &s, &l);
        v = (char *)Mmalloc(l + 1);
        if (l)
            memcpy(v, s, l);
        v[l] = '\0';

        Mfree(c);
    }

    *key = k;
    *value = v;
    return 1;
}

static void _TrimSpace(const char *str, const char **start, size_t *len)
{
    size_t i, j, k;

    k = strlen(str);
    if (!k)
    {
        *len = 0;
        return;
    }

    i = 0;
    j = k - 1;

    k -= 1;
    while (i < k)
        if (str[i] == ' ')
            i += 1;
        else
            break;
    k += 1;

    while (j > 0)
        if (str[j] == ' ')
            j -= 1;
        else
            break;

    if (i > j)
    {
        *len = 0;
        return;
    }
    else if (i == j)
    {
        if (str[i] == ' ')
        {
            *len = 0;
            return;
        }
    }

    *len = j - i + 1;
    *start = str + i;
}

static int _CheckBoth(TC_t **container, void ***ptr, unsigned int *flags, unsigned int lineCount)
{
    TC_t *clients = container[0], *servers = container[1];
    SynchronizationClient_t *client = *(SynchronizationClient_t **)(ptr[0]);
    SynchronizationServer_t *server = *(SynchronizationServer_t **)(ptr[1]);
    int r;

    if (client && server)
        goto _CheckBoth_failed;
    if (!client && !server)
        return 1;

    if (client)
    {
        if ((r = _CheckClientAndAdd(clients, client, flags, lineCount)))
        {
            *(SynchronizationClient_t **)(ptr[0]) = NULL;
            return r;
        }
    }

    if (server)
    {
        if ((r = _CheckServerAndAdd(servers, server, flags, lineCount)))
        {
            *(SynchronizationServer_t **)(ptr[1]) = NULL;
            return r;
        }
    }

_CheckBoth_failed:
    TCTravase(clients, NULL, _CheckFailClearUpClients);
    TCTravase(servers, NULL, _CheckFailClearUpServers);
    if (client)
        Mfree(client);
    if (server)
        Mfree(server);
    *(SynchronizationClient_t **)(ptr[0]) = NULL;
    *(SynchronizationServer_t **)(ptr[1]) = NULL;
    return 0;
}

static int _CheckClientAndAdd(TC_t *clients, SynchronizationClient_t *client, unsigned int *flags, unsigned int lineCount)
{
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_BASE_PATH_SET))
        goto _CheckClientAndAdd_failed;
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_REMOTE_IP_SET))
        goto _CheckClientAndAdd_failed;
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_REMOTE_PORT_SET))
        goto _CheckClientAndAdd_failed;
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_MAGIC_NUMBER_SET))
        goto _CheckClientAndAdd_failed;

    TCAdd(clients, client);
    *flags = 0;
    return 1;

_CheckClientAndAdd_failed:
    if (_FLAG_ISSET(*flags, _CONFIG_FLAG_BASE_PATH_SET))
        Mfree(client->basePath);
    if (_FLAG_ISSET(*flags, _CONFIG_FLAG_REMOTE_IP_SET))
        Mfree(client->remoteIP);
    Mfree(client);
    *flags = 0;
    fprintf(stderr, "[Configurer] Line %u: Invalid client configuraion\n", lineCount);
    return 0;
}

static int _CheckServerAndAdd(TC_t *servers, SynchronizationServer_t *server, unsigned int *flags, unsigned int lineCount)
{
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_BASE_PATH_SET))
        goto _CheckServerAndAdd_failed;
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_MAGIC_NUMBER_SET))
        goto _CheckServerAndAdd_failed;
    if (!_FLAG_ISSET(*flags, _CONFIG_FLAG_LISTENING_PORT_SET))
        goto _CheckServerAndAdd_failed;

    TCAdd(servers, server);
    *flags = 0;
    return 1;

_CheckServerAndAdd_failed:
    if (_FLAG_ISSET(*flags, _CONFIG_FLAG_BASE_PATH_SET))
        Mfree(server->basePath);
    Mfree(server);
    *flags = 0;
    fprintf(stderr, "[Configurer] Line %u: Invalid server configuraion\n", lineCount);
    return 0;
}

static void _CheckFailClearUpClients(void *data, void *param)
{
    param = param;
    _ReleaseClient((SynchronizationClient_t *)data);
    Mfree(data);
}

static void _CheckFailClearUpServers(void *data, void *param)
{
    param = param;
    _ReleaseServer((SynchronizationServer_t *)data);
    Mfree(data);
}

static void _ReadConfigString(char **pair, char **dest, unsigned int *flags, int *ret, unsigned int configFlag, unsigned int lineCount)
{
    if (dest)
    {
        if (_FLAG_ISSET(*flags, configFlag))
        {
            Mfree(*dest);
            fprintf(stderr, "[Configurer] Line %u: Warning: Key \"%s\" is already set.\n", lineCount, pair[0]);
        }
        else
            _FLAG_SET(*flags, configFlag);

        *dest = SDup(pair[1]);
    }
    else
    {
        fprintf(stderr, "[Configurer] Line %u: Unexpected key \"%s\".\n", lineCount, pair[0]);
        *ret = -1;
    }
}

static void _ReadConfigUInt(char **pair, unsigned int *dest, unsigned int *flags, int *ret, unsigned int configFlag, unsigned int lineCount)
{
    if (dest)
    {
        if (_FLAG_ISSET(*flags, configFlag))
            fprintf(stderr, "[Configurer] Line %u: Warning: Key \"%s\" is already set.\n", lineCount, pair[0]);
        else
            _FLAG_SET(*flags, configFlag);

        if (sscanf(pair[1], "%u", dest) != 1)
        {
            fprintf(stderr, "[Configurer] Line %u: Value \"%s\" for Key \"%s\" is invalid.\n", lineCount, pair[1], pair[0]);
            *ret = -1;
        }
    }
    else
    {
        fprintf(stderr, "[Configurer] Line %u: Unexpected key \"%s\".\n", lineCount, pair[0]);
        *ret = -1;
    }
}

static void _PrintClient(SynchronizationClient_t *client)
{
    printf("Base: %s\nRemote IP: %s\nRemote Port: %u\nMagic Number: %u\n\n", client->basePath, client->remoteIP, client->remotePort, client->magicNumber);
}

static void _PrintServer(SynchronizationServer_t *server)
{
    printf("Base: %s\nPort: %u\nMagic Number:%u\n\n", server->basePath, server->listeningPort, server->magicNumber);
}
