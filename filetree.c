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

static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs, TC_t *FNFiles, TC_t *FNFolders);
static char *_PathConcat(const char *parent, const char *filename);
static int _IsPathSeparator(const char *c);
static int _GetFileStat(const char *fullPath, int *result, FileNodeTypeFile_t *fnfile);
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
    if (t->totalFiles)
        Mfree(t->totalFiles);
    if (t->totalFolders)
        Mfree(t->totalFolders);
}

void FileTreeSetBasePath(FileTree_t *t, const char *basePath)
{
    Mfree(t->basePath);
    t->basePath = SDup(basePath);
}

int FileTreeScan(FileTree_t *t)
{
    TC_t FNs, Files, Folders;
    int r;

    TCInit(&FNs);
    TCInit(&Files);
    TCInit(&Folders);
    r = _FileTreeScanRecursive(t->basePath, &FNs, &Files, &Folders);

    TCTransform(&FNs);
    TCTransform(&Files);
    TCTransform(&Folders);

    t->baseChildrenLen = TCCount(&FNs);
    t->baseChildren = (FileNode_t **)Mmalloc(sizeof(*(t->baseChildren)) * t->baseChildrenLen);
    memcpy(t->baseChildren, FNs.fixedStorage.storage, sizeof(*(t->baseChildren)) * t->baseChildrenLen);

    t->totalFilesLen = TCCount(&Files);
    t->totalFiles = (FileNode_t **)Mmalloc(sizeof(*(t->totalFiles)) * t->totalFilesLen);
    memcpy(t->totalFiles, Files.fixedStorage.storage, sizeof(*(t->totalFiles)) * t->totalFilesLen);

    t->totalFoldersLen = TCCount(&Folders);
    t->totalFolders = (FileNode_t **)Mmalloc(sizeof(*(t->totalFolders)) * t->totalFoldersLen);
    memcpy(t->totalFolders, Folders.fixedStorage.storage, sizeof(*(t->totalFolders)) * t->totalFoldersLen);

    TCDeInit(&FNs);
    TCDeInit(&Files);
    TCDeInit(&Folders);
    return r;
}

void FileTreeDebugPrint(FileTree_t *t)
{
    size_t i;

    printf("Path = \"%s\"\nTotal Files = %u\nTotal Folders = %u\n\n", t->basePath, (unsigned int)t->totalFilesLen, (unsigned int)t->totalFoldersLen);
    if (t->baseChildren)
        for (i = 0; i < t->baseChildrenLen; i += 1)
            _PrintFileNode(t->baseChildren[i], 0);
}

// Private functions definition
static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs, TC_t *FNFiles, TC_t *FNFolders)
{
    TC_t DIRs, SubDIR;
    FileNodeTypeFile_t fnfile;
    FileNode_t *fn;
    DIR *dirp;
    struct dirent *dp;
    char *fileFullPath;
    size_t i, j, n, l;
    FILE *f;
    int r = 0, s, ft;
    uint32_t crc32;

    TCInit(&DIRs);
    if (fullPath)
    {
        l = strlen(fullPath);
        if (!l)
            dirp = opendir(".");
        else
            dirp = opendir(fullPath);
    }
    else
        dirp = opendir(".");

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
                if (!_GetFileStat(fileFullPath, &ft, &fnfile))
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
                                fn->fullName = SDup(fileFullPath);
                                fnfile.crc32 = crc32;
                                memcpy(&(fn->file), &fnfile, sizeof(fn->file));
                                TCAdd(FNs, fn);
                                TCAdd(FNFiles, fn);
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
                        fn->fullName = SDup(fileFullPath);
                        fn->flags |= _FILENODE_FLAG_IS_DIR;
                        TCAdd(FNs, fn);
                        TCAdd(&DIRs, fn);
                        TCAdd(FNFolders, fn);
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
        s = _FileTreeScanRecursive(fileFullPath, &SubDIR, FNFiles, FNFolders);
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

static int _IsPathSeparator(const char *c)
{
    if (*c == '/')
        return 1;
    else if (*c == '\\')
        return 1;
    else
        return 0;
}

static int _GetFileStat(const char *fullPath, int *result, FileNodeTypeFile_t *fnfile)
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

    fnfile->size = (size_t)s.st_size;
    fnfile->timeLastModification = s.st_mtime;

    return 0;
}

static void _DestoryFileNode(FileNode_t *fn)
{
    size_t i;

    Mfree(fn->nodeName);
    Mfree(fn->fullName);
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

    printf("Node: \"%s\"\nFull: \"%s\"\nType: 0x%02x\nParent: \"%s\"\n", fn->nodeName, fn->fullName, (unsigned int)fn->flags, (fn->parent) ? (fn->parent->nodeName) : ("<NONE>"));
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
        printf("CRC32: 0x%08x\nMTime: %s\nSize: %u\n\n", fn->file.crc32, buf, (unsigned int)fn->file.size);
    }
}
