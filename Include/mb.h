#ifndef _MB_H_LOADED
#define _MB_H_LOADED

/* size_t */
#include <stddef.h>

/* uint32_t */
#include <stdint.h>

typedef struct
{
    void *ptr;
    size_t size;
} MemoryBlock_t;

/* Duplicate memory block m. Must be released by call to MBfree(). */
void MDup(MemoryBlock_t *dst, const MemoryBlock_t *m);

/* Concatenate two memory blocks. Must be released by call to MBfree(). */
void MConcat(MemoryBlock_t *dst, const MemoryBlock_t *m1, const MemoryBlock_t *m2);

/* Concatenate multiple memory blocks. Must be released by call to MBfree(). */
void MMConcat(MemoryBlock_t *dst, size_t n, ...);

/* Concatenate multiple memory blocks. Must be released by call to MBfree(). */
void MMConcatA(MemoryBlock_t *dst, size_t n, MemoryBlock_t *array);

/* Release a memory block */
void MBfree(MemoryBlock_t *m);

/* Write a 32-bit unsigned integer */
void MWriteU32(void *ptr, uint32_t v);

/* Read a 32-bit unsigned integer */
uint32_t MReadU32(void **ptr);

/* Write a 64-bit unsigned integer */
void MWriteU64(void *ptr, uint64_t v);

/* Read a 64-bit unsigned integer */
uint64_t MReadU64(void **ptr);

/* Write a string to memory block. Must be released by call to MBfree().*/
void MWriteString(MemoryBlock_t *dst, const char *s);

/* Read a string from memory block. Must be released by call to Mfree(). */
char *MReadString(void **ptr, size_t *maxLength);

#endif
