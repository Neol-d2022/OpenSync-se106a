#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mm.h"
#include "netwprot.h"

static int _RawReadSocket(SOCKET s, void *buf, int desiredLength, int *actualLength, struct timeval *timeout);
static int _ReadUint16(SOCKET s, uint16_t *out, struct timeval *timeout);
static void _BufferToUInt16(unsigned char *buf, size_t length, uint16_t *out);
static unsigned char *_ReadRawData(SOCKET s, int length, struct timeval *timeout);
static int _RawWriteSocket(SOCKET s, const void *buf, int length);
static int _SendUInt16(SOCKET s, uint16_t data);

int NetwProtReadFrom(SOCKET s, SocketMessage_t *sm, struct timeval *timeout)
{
    if (_ReadUint16(s, &(sm->messageType), timeout))
        return 1;
    if (_ReadUint16(s, &(sm->messageLength), timeout))
        return 1;
    if ((sm->message = _ReadRawData(s, (int)(sm->messageLength), timeout)) == NULL)
        return 1;
    return 0;
}

int NetwProtSendTo(SOCKET s, const SocketMessage_t *sm)
{
    if (_SendUInt16(s, sm->messageType))
        return 1;
    if (_SendUInt16(s, sm->messageLength))
        return 1;
    if (_RawWriteSocket(s, sm->message, sm->messageLength))
        return 1;
    return 0;
}

void NetwProtFreeSocketMesg(SocketMessage_t *sm)
{
    Mfree(sm->message);
}

// ==========================
// Local Function Definitions
// ==========================

static int _RawReadSocket(SOCKET s, void *buf, int desiredLength, int *actualLength, struct timeval *timeout)
{
    fd_set fset;
    struct timeval tv;
    int r, byteRead, ret = 0;

    byteRead = 0;
    if (desiredLength <= 0)
        return 1;
    while (byteRead < desiredLength)
    {
        if (timeout)
        {
            FD_ZERO(&fset);
            FD_SET(s, &fset);
            memcpy(&tv, timeout, sizeof(tv));
            r = select(s + 1, &fset, NULL, NULL, &tv);
            if (r < 0)
            {
                ret = 1;
                break;
            }
            else if (r == 0)
                break;
        }

        r = recv(s, buf + byteRead, desiredLength - byteRead, 0);
        if (r < 0)
        {
            ret = 1;
            break;
        }
        else if (r == 0)
            break;
        else if (r > desiredLength)
            abort();
        byteRead += r;
    }

    *actualLength = byteRead;
    return ret;
}

static int _ReadUint16(SOCKET s, uint16_t *out, struct timeval *timeout)
{
    unsigned char buf[sizeof(*out)];
    int retLength;

    if (_RawReadSocket(s, buf, sizeof(buf), &retLength, timeout))
        return 1;
    if (retLength != sizeof(buf))
        return 1;
    _BufferToUInt16(buf, sizeof(buf), out);
    return 0;
}

static void _BufferToUInt16(unsigned char *buf, size_t length, uint16_t *out)
{
    size_t i;
    uint16_t r = 0;

    for (i = 0; i < length; i += 1)
    {
        r = (uint16_t)(r << 8);
        r += buf[i];
    }

    *out = r;
}

static unsigned char *_ReadRawData(SOCKET s, int length, struct timeval *timeout)
{
    unsigned char *buf;
    int actualLength;

    buf = (unsigned char *)Mmalloc(length);
    if (_RawReadSocket(s, buf, length, &actualLength, timeout))
        goto _ReadRawData_Fail;
    if (length != actualLength)
        goto _ReadRawData_Fail;
    return buf;

_ReadRawData_Fail:
    Mfree(buf);
    return NULL;
}

static int _RawWriteSocket(SOCKET s, const void *buf, int length)
{
    int r;

    if (length < 0)
        return 1;
    r = send(s, buf, length, 0);
    if (r <= 0)
        return 1;
    if (r != length)
        return 1;
    return 0;
}

static int _SendUInt16(SOCKET s, uint16_t data)
{
    size_t i;
    unsigned char buf[sizeof(data)];

    for (i = 0; i < sizeof(buf); i += 1)
    {
        buf[sizeof(buf) - i - 1] = (unsigned char)(data & 0xFF);
        data = (uint16_t)(data >> 8);
    }

    return _RawWriteSocket(s, buf, sizeof(buf));
}
