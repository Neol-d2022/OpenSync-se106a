#ifndef _MM_H_LOADED
#define _MM_H_LOADED

/* size_t */
#include <stddef.h>

/* Request a memory block with s bytes */
void *Mmalloc(size_t s);

/* Relocate memory block p to s bytes */
void *Mrealloc(void *p, size_t s);

/* When the system is out of memory, the functions above will immediately crash the program, instead of return NULL */

/* Release memory block p */
void Mfree(void *p);

/* DEBUG. Return the number of allocated memory blocks */
size_t MDebug();

#endif
