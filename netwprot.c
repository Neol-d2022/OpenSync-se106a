#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mm.h"
#include "netwprot.h"

static int _RawReadSocket(SOCKET s, void *buf, int desiredLength, int *actualLength, struct timeval *timeout);
static int _ReadUint16(SOCKET s, uint16_t *out, struct timeval *timeout);
static int _ReadUint32(SOCKET s, uint32_t *out, struct timeval *timeout);
static unsigned char *_ReadRawData(SOCKET s, int length, struct timeval *timeout);
static int _RawWriteSocket(SOCKET s, const void *buf, int length);
static int _SendUInt16(SOCKET s, uint16_t data);
static int _SendUInt32(SOCKET s, uint32_t data);

int NetwProtReadFrom(SOCKET s, SocketMessage_t *sm, struct timeval *timeout)
{
    if (_ReadUint16(s, &(sm->messageType), timeout))
        return 1;
    if (_ReadUint32(s, &(sm->messageLength), timeout))
        return 1;
    if ((sm->message = _ReadRawData(s, (int)(sm->messageLength), timeout)) == NULL)
        return 1;
    return 0;
}

int NetwProtSendTo(SOCKET s, const SocketMessage_t *sm)
{
    if (_SendUInt16(s, sm->messageType))
        return 1;
    if (_SendUInt32(s, sm->messageLength))
        return 1;
    if (_RawWriteSocket(s, sm->message, sm->messageLength))
        return 1;
    return 0;
}

void NetwProtFreeSocketMesg(SocketMessage_t *sm)
{
    Mfree(sm->message);
}

void NetwProtUInt8ToBuf(unsigned char *buf, uint8_t data)
{
    size_t i;

    for (i = 0; i < sizeof(buf); i += 1)
    {
        buf[sizeof(buf) - i - 1] = (unsigned char)(data & 0xFF);
        data = (uint8_t)(data >> 8);
    }
}

void NetwProtBufToUInt8(const unsigned char *buf, uint8_t *out)
{
    size_t i;
    uint8_t r = 0;

    for (i = 0; i < sizeof(*out); i += 1)
    {
        r = (uint8_t)(r << 8);
        r += buf[i];
    }

    *out = r;
}

void NetwProtUInt16ToBuf(unsigned char *buf, uint16_t data)
{
    size_t i;

    for (i = 0; i < sizeof(data); i += 1)
    {
        buf[sizeof(data) - i - 1] = (unsigned char)(data & 0xFF);
        data = (uint16_t)(data >> 8);
    }
}

void NetwProtBufToUInt16(const unsigned char *buf, uint16_t *out)
{
    size_t i;
    uint16_t r = 0;

    for (i = 0; i < sizeof(*out); i += 1)
    {
        r = (uint16_t)(r << 8);
        r += buf[i];
    }

    *out = r;
}

void NetwProtUInt32ToBuf(unsigned char *buf, uint32_t data)
{
    size_t i;

    for (i = 0; i < sizeof(data); i += 1)
    {
        buf[sizeof(data) - i - 1] = (unsigned char)(data & 0xFF);
        data = (uint32_t)(data >> 8);
    }
}

void NetwProtBufToUInt32(const unsigned char *buf, uint32_t *out)
{
    size_t i;
    uint32_t r = 0;

    for (i = 0; i < sizeof(*out); i += 1)
    {
        r = (uint32_t)(r << 8);
        r += buf[i];
    }

    *out = r;
}

void NetwProtSetSM(SocketMessage_t *sm, uint16_t type, uint32_t length, unsigned char *mesg)
{
    sm->messageType = type;
    sm->messageLength = length;
    sm->message = mesg;
}

int NetwProtSendFile(SOCKET s, const char *filepath)
{
    unsigned char buf[NETWPROT_FILE_TRANSFER_BUFFER_SIZE];
    struct stat st;
    FILE *f;
    size_t sent, total, expected, actual;
    int r = 0;
    uint32_t fileSize;

    r = stat(filepath, &st);
    if (r)
        return 1;
    total = (size_t)st.st_size;
    fileSize = (uint32_t)total;
    r = _SendUInt32(s, fileSize);
    if (r)
        return r;

    f = fopen(filepath, "rb");
    if (!f)
        return 1;

    sent = 0;
    while (sent < total)
    {
        if (sent + sizeof(buf) > total)
            expected = total - sent;
        else
            expected = sizeof(buf);
        actual = fread(buf, 1, expected, f);
        if (actual != expected)
        {
            r = 1;
            break;
        }
        r = _RawWriteSocket(s, buf, actual);
        if (r)
            break;
        sent += actual;
    }

    fclose(f);
    return r;
}

int NetwProtRecvFile(SOCKET s, const char *savefilepath, struct timeval *timeout)
{
    unsigned char *buf;
    FILE *f;
    size_t received, total, expected, actual;
    uint32_t fileSize;
    int r;

    r = _ReadUint32(s, &fileSize, timeout);
    if (r)
        return r;
    total = (size_t)fileSize;

    f = fopen(savefilepath, "wb");
    if (!f)
        return 1;

    received = 0;
    while (received < total)
    {
        if (received + NETWPROT_FILE_TRANSFER_BUFFER_SIZE > total)
            expected = total - received;
        else
            expected = NETWPROT_FILE_TRANSFER_BUFFER_SIZE;
        buf = _ReadRawData(s, expected, timeout);
        if (!buf)
        {
            fclose(f);
            return 1;
        }
        actual = fwrite(buf, 1, expected, f);
        Mfree(buf);
        if (actual != expected)
        {
            fclose(f);
            return 1;
        }
        received += actual;
    }

    fclose(f);
    return 0;
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
    NetwProtBufToUInt16(buf, out);
    return 0;
}

static int _ReadUint32(SOCKET s, uint32_t *out, struct timeval *timeout)
{
    unsigned char buf[sizeof(*out)];
    int retLength;

    if (_RawReadSocket(s, buf, sizeof(buf), &retLength, timeout))
        return 1;
    if (retLength != sizeof(buf))
        return 1;
    NetwProtBufToUInt32(buf, out);
    return 0;
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
    unsigned char buf[sizeof(data)];
    NetwProtUInt16ToBuf(buf, data);

    return _RawWriteSocket(s, buf, sizeof(buf));
}

static int _SendUInt32(SOCKET s, uint32_t data)
{
    unsigned char buf[sizeof(data)];
    NetwProtUInt32ToBuf(buf, data);

    return _RawWriteSocket(s, buf, sizeof(buf));
}
