#include <string.h>
#include <stdarg.h>

#include "mb.h"
#include "mm.h"

static void _MMConcatA(MemoryBlock_t *dst, size_t n, MemoryBlock_t *array, size_t totalLength);

void MDup(MemoryBlock_t *dst, const MemoryBlock_t *m)
{
    void *r;
    size_t l = m->size;

    r = Mmalloc(l);
    memcpy(r, m->ptr, l);

    dst->ptr = r;
    dst->size = l;
}

void MConcat(MemoryBlock_t *dst, const MemoryBlock_t *m1, const MemoryBlock_t *m2)
{
    return MMConcat(dst, 2, m1, m2);
}

void MMConcat(MemoryBlock_t *dst, size_t n, ...)
{
    MemoryBlock_t *mbs;
    size_t i, c;
    va_list list;

    c = 0;
    va_start(list, n);
    mbs = (MemoryBlock_t *)Mmalloc(sizeof(*mbs) * n);
    for (i = 0; i < n; i += 1)
    {
        mbs[i] = *va_arg(list, const MemoryBlock_t *);
        c += mbs[i].size;
    }
    va_end(list);

    _MMConcatA(dst, n, mbs, c);
    Mfree(mbs);
}

void MMConcatA(MemoryBlock_t *dst, size_t n, MemoryBlock_t *array)
{
    size_t i, c;

    c = 0;
    for (i = 0; i < n; i += 1)
        c += array[i].size;

    _MMConcatA(dst, n, array, c);
}

void MBfree(MemoryBlock_t *m)
{
    Mfree(m->ptr);
}

void MWriteU32(void *ptr, uint32_t v)
{
    uint8_t w[] = {
        (v & 0xFF000000) >> 24,
        (v & 0x00FF0000) >> 16,
        (v & 0x0000FF00) >> 8,
        (v & 0x000000FF)};
    size_t i;
    uint8_t *p;

    p = (uint8_t *)ptr;
    for (i = 0; i < sizeof(w) / sizeof(w[0]); i += 1)
        p[i] = w[i];
}

uint32_t MReadU32(void **ptr)
{
    uint8_t w[4];
    size_t i;
    uint8_t *p;

    p = (uint8_t *)(*ptr);
    for (i = 0; i < sizeof(w) / sizeof(w[0]); i += 1)
        w[i] = *(p++);

    *ptr = p;
    return (((uint32_t)w[0] << 24) | ((uint32_t)w[1] << 16) | ((uint32_t)w[2] << 8) | ((uint32_t)w[3]));
}

void MWriteU64(void *ptr, uint64_t v)
{
    uint8_t w[] = {
        (v & 0xFF00000000000000) >> 56,
        (v & 0x00FF000000000000) >> 48,
        (v & 0x0000FF0000000000) >> 40,
        (v & 0x000000FF00000000) >> 32,
        (v & 0x00000000FF000000) >> 24,
        (v & 0x0000000000FF0000) >> 16,
        (v & 0x000000000000FF00) >> 8,
        (v & 0x00000000000000FF)};
    size_t i;
    uint8_t *p;

    p = (uint8_t *)ptr;
    for (i = 0; i < sizeof(w) / sizeof(w[0]); i += 1)
        p[i] = w[i];
}

uint64_t MReadU64(void **ptr)
{
    uint8_t w[8];
    size_t i;
    uint8_t *p;

    p = (uint8_t *)(*ptr);
    for (i = 0; i < sizeof(w) / sizeof(w[0]); i += 1)
        w[i] = *(p++);

    *ptr = p;
    return (((uint64_t)w[0] << 56) | ((uint64_t)w[1] << 48) | ((uint64_t)w[2] << 40) | ((uint64_t)w[3] << 32) | ((uint64_t)w[4] << 24) | ((uint64_t)w[5] << 16) | ((uint64_t)w[6] << 8) | ((uint64_t)w[7]));
}

void MWriteString(MemoryBlock_t *dst, const char *s)
{
    void *p;
    size_t l, t;

    l = strlen(s);
    t = l + sizeof(uint32_t);

    p = Mmalloc(t);
    MWriteU32(p, (uint32_t)l);
    memcpy(p + sizeof(uint32_t), s, l);

    dst->ptr = p;
    dst->size = t;
}

char *MReadString(void **ptr, size_t *maxLength)
{
    char *s;
    size_t len;
    uint32_t l;

    if ((*maxLength) < sizeof(l))
        return NULL;

    l = MReadU32(ptr);
    (*maxLength) -= sizeof(l);
    len = (size_t)l;
    if ((*maxLength) < len)
        return NULL;

    s = (char *)Mmalloc(len + 1);
    memcpy(s, *ptr, len);
    s[len] = '\0';
    (*ptr) = (char *)(*ptr) + len;
    (*maxLength) -= len;

    return s;
}

// ==========================
// Local function definitions
// ==========================

static void _MMConcatA(MemoryBlock_t *dst, size_t n, MemoryBlock_t *array, size_t totalLength)
{
    void *r;
    size_t i, d;

    r = Mmalloc(totalLength);
    d = 0;
    for (i = 0; i < n; i += 1)
    {
        memcpy(r + d, array[i].ptr, array[i].size);
        d += array[i].size;
    }

    dst->ptr = r;
    dst->size = d;
}
