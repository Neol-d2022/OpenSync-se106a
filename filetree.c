#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

#include "filetree.h"
#include "mm.h"
#include "strings.h"
#include "transformcontainer.h"
#include "crc32.h"

#ifdef _WIN32
#define kPathSeparator '\\'
#else
#define kPathSeparator '/'
#endif

#define _FILENODE_FLAG_IS_DIR 0x01

#define _FILE_TYPE_UNKNOWN 0
#define _FILE_TYPE_REGULAR 1
#define _FILE_TYPE_FOLDER 2

static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs);
static char *_PathConcat(const char *parent, const char *filename);
static int _IsPathSeparator(const char *c);
static int _GetFileStat(const char *fullPath, int *result, time_t *timeLastModification);
static void _DestoryFileNode(FileNode_t *fn);
static void _PrintFileNode(FileNode_t *fn, size_t depth);

void FileTreeInit(FileTree_t *t)
{
    memset(t, 0, sizeof(*t));
    t->basePath = SDup(".");
}

void FileTreeDeInit(FileTree_t *t)
{
    size_t i;

    Mfree(t->basePath);
    if (t->baseChildren)
    {
        for (i = 0; i < t->baseChildrenLen; i += 1)
        {
            _DestoryFileNode(t->baseChildren[i]);
            Mfree(t->baseChildren[i]);
        }
        Mfree(t->baseChildren);
    }
}

void FileTreeSetBasePath(FileTree_t *t, const char *basePath)
{
    Mfree(t->basePath);
    t->basePath = SDup(basePath);
}

int FileTreeScan(FileTree_t *t)
{
    TC_t FNs;
    int r;

    TCInit(&FNs);
    r = _FileTreeScanRecursive(t->basePath, &FNs);

    TCTransform(&FNs);
    t->baseChildrenLen = TCCount(&FNs);
    t->baseChildren = (FileNode_t **)Mmalloc(sizeof(*(t->baseChildren)) * t->baseChildrenLen);
    memcpy(t->baseChildren, FNs.fixedStorage.storage, sizeof(*(t->baseChildren)) * t->baseChildrenLen);

    TCDeInit(&FNs);
    return r;
}

void FileTreeDebugPrint(FileTree_t *t)
{
    size_t i;

    printf("Path = \"%s\"\n", t->basePath);
    if (t->baseChildren)
        for (i = 0; i < t->baseChildrenLen; i += 1)
            _PrintFileNode(t->baseChildren[i], 0);
}

// Private functions definition
static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs)
{
    TC_t DIRs, SubDIR;
    time_t mtime;
    FileNode_t *fn;
    DIR *dirp;
    struct dirent *dp;
    char *fileFullPath;
    size_t i, j, n;
    FILE *f;
    int r = 0, s, ft;
    uint32_t crc32;

    TCInit(&DIRs);
    dirp = opendir(fullPath);
    if (dirp)
        do
        {
            errno = 0;
            if ((dp = readdir(dirp)) != NULL)
            {
                if (strcmp(dp->d_name, ".") == 0)
                    continue;
                else if (strcmp(dp->d_name, "..") == 0)
                    continue;

                fileFullPath = _PathConcat(fullPath, dp->d_name);
                if (!_GetFileStat(fileFullPath, &ft, &mtime))
                {
                    switch (ft)
                    {
                    case _FILE_TYPE_REGULAR:
                        f = fopen(fileFullPath, "rb");
                        if (f)
                        {
                            if (Crc32_ComputeFile(f, &crc32) == 0)
                            {
                                fn = (FileNode_t *)Mmalloc(sizeof(*fn));
                                memset(fn, 0, sizeof(*fn));
                                fn->nodeName = SDup(dp->d_name);
                                fn->file.crc32 = crc32;
                                fn->file.timeLastModification = mtime;
                                TCAdd(FNs, fn);
                            }
                            else
                                r = errno;
                            fclose(f);
                        }
                        else
                            r = errno;

                        break;
                    case _FILE_TYPE_FOLDER:
                        fn = (FileNode_t *)Mmalloc(sizeof(*fn));
                        memset(fn, 0, sizeof(*fn));
                        fn->nodeName = SDup(dp->d_name);
                        fn->flags |= _FILENODE_FLAG_IS_DIR;
                        TCAdd(FNs, fn);
                        TCAdd(&DIRs, fn);
                        break;
                    case _FILE_TYPE_UNKNOWN:
                        break;
                    }
                }
                Mfree(fileFullPath);
            }
            else if (errno != 0)
                r = errno;
        } while (dp);
    else
        r = errno;
    closedir(dirp);

    TCTransform(&DIRs);
    n = TCCount(&DIRs);
    for (i = 0; i < n; i += 1)
    {
        fn = (FileNode_t *)TCI(&DIRs, i);
        fileFullPath = _PathConcat(fullPath, fn->nodeName);
        TCInit(&SubDIR);
        s = _FileTreeScanRecursive(fileFullPath, &SubDIR);
        if (s)
            r = s;
        TCTransform(&SubDIR);
        fn->folder.childrenLen = TCCount(&SubDIR);
        fn->folder.children = (FileNode_t **)Mmalloc(sizeof(*(fn->folder.children)) * (fn->folder.childrenLen));
        memcpy(fn->folder.children, SubDIR.fixedStorage.storage, sizeof(*(fn->folder.children)) * (fn->folder.childrenLen));
        for (j = 0; j < fn->folder.childrenLen; j += 1)
            (fn->folder.children)[j]->parent = fn;
        TCDeInit(&SubDIR);
        Mfree(fileFullPath);
    }

    TCDeInit(&DIRs);
    return r;
}

static char *_PathConcat(const char *parent, const char *filename)
{
    static const char kPathSeparatorString[] = {kPathSeparator, '\0'};
    size_t a, b;
    unsigned int nSeparators = 0;

    a = strlen(parent);
    b = strlen(filename);

    if (!a || !b)
        abort();

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

static int _IsPathSeparator(const char *c)
{
    if (*c == '/')
        return 1;
    else if (*c == '\\')
        return 1;
    else
        return 0;
}

static int _GetFileStat(const char *fullPath, int *result, time_t *timeLastModification)
{
    struct stat s;
    int r;

    r = stat(fullPath, &s);
    if (r)
        return r;

    if (_S_ISREG(s.st_mode))
        *result = _FILE_TYPE_REGULAR;
    else if (_S_ISDIR(s.st_mode))
        *result = _FILE_TYPE_FOLDER;
    else
        *result = _FILE_TYPE_UNKNOWN;

    *timeLastModification = s.st_mtime;

    return 0;
}

static void _DestoryFileNode(FileNode_t *fn)
{
    size_t i;

    Mfree(fn->nodeName);
    if (fn->flags & _FILENODE_FLAG_IS_DIR)
    {
        if (fn->folder.children)
        {
            for (i = 0; i < fn->folder.childrenLen; i += 1)
            {
                _DestoryFileNode((fn->folder.children)[i]);
                Mfree((fn->folder.children)[i]);
            }
            Mfree(fn->folder.children);
        }
    }
}

static void _PrintFileNode(FileNode_t *fn, size_t depth)
{
    char buf[64];
    struct tm t;
    size_t i;

    printf("Node: \"%s\"\nType: 0x%02x\nParent: \"%s\"\n", fn->nodeName, (unsigned int)fn->flags, (fn->parent) ? (fn->parent->nodeName) : ("<NONE>"));
    if (fn->flags & _FILENODE_FLAG_IS_DIR)
    {
        printf("Children count: %u\n\n", (unsigned int)fn->folder.childrenLen);
        if (fn->folder.children)
            for (i = 0; i < fn->folder.childrenLen; i += 1)
                _PrintFileNode((fn->folder.children)[i], depth + 1);
    }
    else
    {
        t = *localtime(&(fn->file.timeLastModification));
        strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &t);
        printf("CRC32: 0x%08x\nMTime: %s\n\n", fn->file.crc32, buf);
    }
}
