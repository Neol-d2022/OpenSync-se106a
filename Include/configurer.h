#ifndef _CONFIGURER_H_LOADED
#define _CONFIGURER_H_LOADED

/* size_t */
#include <stddef.h>

/* pthread_t */
#include <pthread.h>

/* SynchronizationProtocols_t */
#include "syncprot.h"

typedef struct
{
    SynchronizationProtocols_t *sp;
    char *workingFolder;
    char *basePath;
    char *remoteIP;
    unsigned int remotePort;
    unsigned int magicNumber;
} SynchronizationClient_t;

typedef struct
{
    SynchronizationProtocols_t *sp;
    char *workingFolder;
    char *basePath;
    unsigned int listeningPort;
    unsigned int magicNumber;
} SynchronizationServer_t;

typedef struct
{
    SynchronizationProtocols_t *sp;
    size_t nClients;
    size_t nServers;
    SynchronizationClient_t **clients;
    SynchronizationServer_t **servers;
    pthread_t *clientThreads;
    pthread_t *serverThreads;
} Configuration_t;

int ConfigurerReadConfig(const char *filename, Configuration_t *c);
void ConfigurerRelease(Configuration_t *c);
void ConfigurerDebugPrint(Configuration_t *c);
int ConfigurerStartup(Configuration_t *c);

#endif
