#ifndef _CONFIGURER_H_LOADED
#define _CONFIGURER_H_LOADED

/* size_t */
#include <stddef.h>

typedef struct
{
    char *basePath;
    char *remoteIP;
    unsigned int remotePort;
    unsigned int magicNumber;
} SynchronizationClient_t;

typedef struct
{
    char *basePath;
    unsigned int listeningPort;
    unsigned int magicNumber;
} SynchronizationServer_t;

typedef struct
{
    size_t nClients;
    size_t nServers;
    SynchronizationClient_t **clients;
    SynchronizationServer_t **servers;
} Configuration_t;

int ConfigurerReadConfig(const char *filename, Configuration_t *c);
void ConfigurerRelease(Configuration_t *c);
void ConfigurerDebugPrint(Configuration_t *c);

#endif
