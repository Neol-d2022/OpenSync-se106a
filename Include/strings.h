#ifndef _STRINGS_H_LOADED
#define _STRINGS_H_LOADED

/* size_t */
#include <stddef.h>

/* Duplicate string str. Must be released by call to Mfree(). */
char *SDup(const char *str);

/* Concatenate two strings. Must be released by call to Mfree(). */
char *SConcat(const char *s1, const char *s2);

/* Concatenate multiple strings. Must be released by call to Mfree(). */
char *SMConcat(size_t n, ...);

/* Concatenate multiple strings. Must be released by call to Mfree(). */
char *SMConcatA(size_t n, const char **array);

#endif
