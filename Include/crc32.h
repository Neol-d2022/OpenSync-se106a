#ifndef _CRC32_H_LOADED
#define _CRC32_H_LOADED

#include <stdio.h>
#include <stdint.h>

int Crc32_ComputeFile(FILE *file, uint32_t *outCrc32);
uint32_t Crc32_ComputeBuf(uint32_t inCrc32, const void *buf, size_t bufLen);

#endif
