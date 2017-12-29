#ifndef _DIR_MANAGER_H_LOADED
#define _DIR_MANAGER_H_LOADED

/* remove */
#include <stdlib.h>

#ifdef _WIN32

#define mkdir(dir, mode) _mkdir(dir)

#else //#ifndef _WIN32

/* mkdir */
#include <sys/stat.h>
#include <sys/types.h>

#endif //#ifdef _WIN32
#endif //#ifdef _DIR_MANAGER_H_LOADED

char *DirManagerPathConcat(const char *parent, const char *filename);
