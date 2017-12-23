#ifndef _NETWPROT_H_LOADED
#define _NETWPROT_H_LOADED

/* uint16_t */
#include <stdint.h>

#include "xsocket.h"

#define NETWPROT_READ_TIMEOUT_IN_SECOND 16
#define NETWPROT_IDLE_TIMEOUT_CLIENT_IN_SECOND 20
#define NETWPROT_IDLE_TIMEOUT_SERVER_IN_SECOND 120

#define NETWPROT_SM_TYPE_LENGTH 2
#define NETWPROT_SM_LENGTH_LENGTH 2

#define NETWPROT_SM_MESSAGE_TYPE_RESPONSE 1
#define NETWPROT_SM_MESSAGE_TYPE_HANDSHAKE 2
#define NETWPROT_SM_MESSAGE_TYPE_KEEPALIVE 3
#define NETWPROT_SM_MESSAGE_TYPE_FILETREE_REQUEST 4
#define NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_DELETED 5
#define NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_CREATED 6
#define NETWPROT_SM_MESSAGE_TYPE_REQUEST_FILE 7
#define NETWPROT_SM_MESSAGE_TYPE_NOTIFY_FILE_CHANGED 8
#define NETWPROT_SM_MESSAGE_TYPE_MAX 9

#define NETWPROT_RESPONSE_OK 0

#define NETWPROT_FILE_TRANSFER_BUFFER_SIZE 1024

typedef struct
{
    uint16_t messageType;
    uint32_t messageLength;
    unsigned char *message;
} SocketMessage_t;

int NetwProtReadFrom(SOCKET s, SocketMessage_t *sm, struct timeval *timeout);
int NetwProtSendTo(SOCKET s, const SocketMessage_t *sm);
void NetwProtFreeSocketMesg(SocketMessage_t *sm);
void NetwProtUInt8ToBuf(unsigned char *buf, uint8_t data);
void NetwProtBufToUInt8(const unsigned char *buf, uint8_t *out);
void NetwProtUInt16ToBuf(unsigned char *buf, uint16_t data);
void NetwProtBufToUInt16(const unsigned char *buf, uint16_t *out);
void NetwProtUInt32ToBuf(unsigned char *buf, uint32_t data);
void NetwProtBufToUInt32(const unsigned char *buf, uint32_t *out);
void NetwProtSetSM(SocketMessage_t *sm, uint16_t type, uint32_t length, unsigned char *mesg);
int NetwProtSendFile(SOCKET s, const char *filepath);
int NetwProtRecvFile(SOCKET s, const char *savefilepath, struct timeval *timeout);

#endif
