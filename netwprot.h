#ifndef _NETWPROT_H_LOADED
#define _NETWPROT_H_LOADED

/* uint16_t */
#include <stdint.h>

#include "xsocket.h"

#define NETWPROT_SM_TYPE_LENGTH 2
#define NETWPROT_SM_LENGTH_LENGTH 2
#define NETWPROT_SM_INITIAL_BUFFER_LENGTH ((NETWPROT_SM_TYPE_LENGTH) > (NETWPROT_SM_LENGTH_LENGTH) ? (NETWPROT_SM_TYPE_LENGTH) : (NETWPROT_SM_LENGTH_LENGTH))
typedef struct
{
    uint16_t messageType;
    uint16_t messageLength;
    unsigned char *message;
} SocketMessage_t;

int NetwProtReadFrom(SOCKET s, SocketMessage_t *sm, struct timeval *timeout);
int NetwProtSendTo(SOCKET s, SocketMessage_t *sm);

#endif
