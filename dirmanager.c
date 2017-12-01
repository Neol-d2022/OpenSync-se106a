#include <stdlib.h>
#include <string.h>

#include "dirmanager.h"
#include "strings.h"

#ifdef _WIN32
#define kPathSeparator '\\'
#else
#define kPathSeparator '/'
#endif

static int _IsPathSeparator(const char *c);

char *DirManagerPathConcat(const char *parent, const char *filename)
{
    static const char kPathSeparatorString[] = {kPathSeparator, '\0'};
    size_t a;
    unsigned int nSeparators = 0;

    a = strlen(parent);

    if (a > 0)
        if (_IsPathSeparator(parent + a - 1))
            nSeparators += 1;

    if (_IsPathSeparator(filename))
        nSeparators += 1;

    switch (nSeparators)
    {
    case 0:
        return SMConcat(3, parent, kPathSeparatorString, filename);
        break;
    case 1:
        return SConcat(parent, filename);
        break;
    case 2:
        return SConcat(parent, filename + 1);
        break;
    default:
        abort();
    }

    return NULL;
}

//========
//
//========

static int _IsPathSeparator(const char *c)
{
    if (*c == '/')
        return 1;
    else if (*c == '\\')
        return 1;
    else
        return 0;
}
