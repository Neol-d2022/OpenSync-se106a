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

#define _FILENODE_FLAG_IS_DIR 0x00000001
#define _FILENODE_FLAG_CRC_VALID 0x00000002

#define _FILE_TYPE_UNKNOWN 0
#define _FILE_TYPE_REGULAR 1
#define _FILE_TYPE_FOLDER 2

static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs, TC_t *FNFiles, TC_t *FNFolders);
static char *_PathConcat(const char *parent, const char *filename);
static int _IsPathSeparator(const char *c);
static int _GetFileStat(const char *fullPath, int *result, FileNodeTypeFile_t *fnfile);
static void _DestoryFileNode(FileNode_t *fn);
static void _PrintFileNode(FileNode_t *fn, size_t depth);
static void _FileNodeToMemoryBlock(FileNode_t *fn, MemoryBlock_t *mb);
static FileNode_t *_FileNodeFromMemoryBlock(FileNode_t *parent, const char *parentPath, void **ptr, size_t *maxLength);
static void _FileTreeConstructAfterLoadingFromMemoryBlock(FileTree_t *t);
static void _FileTreeConstructAfterLoadingFromMemoryBlock_Node(FileNode_t *fn, TC_t *files, TC_t *folders);

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

void FileTreeToMemoryblock(FileTree_t *t, MemoryBlock_t *mb)
{
    MemoryBlock_t baseCountM;
    unsigned char baseCountUC[8];
    MemoryBlock_t *results;
    size_t i;

    baseCountM.ptr = baseCountUC;
    baseCountM.size = sizeof(baseCountUC);
    MWriteU64(baseCountUC, t->baseChildrenLen);

    results = Mmalloc(sizeof(*results) * (t->baseChildrenLen + 1));
    memcpy(results + 0, &baseCountM, sizeof(*results));
    for (i = 0; i < t->baseChildrenLen; i += 1)
        _FileNodeToMemoryBlock((t->baseChildren)[i], results + i + 1);

    MMConcatA(mb, t->baseChildrenLen + 1, results);

    for (i = 0; i < t->baseChildrenLen; i += 1)
        MBfree(results + i + 1);
    Mfree(results);
}

FileTree_t *FileTreeFromMemoryBlock(MemoryBlock_t *mb, const char *parentPath)
{
    uint64_t baseCountU64;
    FileTree_t *t;
    FileNode_t *child;
    size_t i;
    void *_ptr, **ptr;
    size_t _maxLength, *maxLength;

    _ptr = mb->ptr;
    ptr = &_ptr;
    _maxLength = mb->size;
    maxLength = &_maxLength;

    if ((*maxLength) < sizeof(baseCountU64))
        return NULL;

    baseCountU64 = MReadU64(ptr);
    (*maxLength) -= sizeof(baseCountU64);

    t = (FileTree_t *)Mmalloc(sizeof(*t));
    t->basePath = SDup(parentPath);
    t->baseChildrenLen = (size_t)baseCountU64;
    t->baseChildren = (FileNode_t **)Mmalloc(sizeof(*(t->baseChildren)) * t->baseChildrenLen);
    for (i = 0; i < t->baseChildrenLen; i += 1)
    {
        child = _FileNodeFromMemoryBlock(NULL, t->basePath, ptr, maxLength);
        if (child == NULL)
        {
            size_t j;
            for (j = 0; j < i; j += 1)
            {
                _DestoryFileNode((t->baseChildren)[j]);
                Mfree((t->baseChildren)[j]);
            }
            Mfree(t->baseChildren);
            Mfree(t->basePath);
            Mfree(t);
            return NULL;
        }
        (t->baseChildren)[i] = child;
    }

    _FileTreeConstructAfterLoadingFromMemoryBlock(t);
    return t;
}

int FileTreeComputeCRC32(FileTree_t *t)
{
    FILE *f;
    size_t i;
    uint32_t crc32;
    int r = 0, s;

    for (i = 0; i < t->totalFilesLen; i += 1)
    {
        f = fopen((t->totalFiles)[i]->fullName, "rb");
        if (f)
        {
            s = Crc32_ComputeFile(f, &crc32);
            if (s)
                r = s;
            else
            {
                (t->totalFiles)[i]->file.crc32 = crc32;
                (t->totalFiles)[i]->flags |= _FILENODE_FLAG_CRC_VALID;
            }
            fclose(f);
        }
        else
            r = errno;
    }

    return r;
}

// ============================
// Private functions definition
// ============================

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
                            fn = (FileNode_t *)Mmalloc(sizeof(*fn));
                            memset(fn, 0, sizeof(*fn));
                            fn->nodeName = SDup(dp->d_name);
                            fn->fullName = SDup(fileFullPath);
                            fn->flags &= (~_FILENODE_FLAG_CRC_VALID);
                            memcpy(&(fn->file), &fnfile, sizeof(fn->file));
                            TCAdd(FNs, fn);
                            TCAdd(FNFiles, fn);
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

    if (S_ISREG(s.st_mode))
        *result = _FILE_TYPE_REGULAR;
    else if (S_ISDIR(s.st_mode))
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

    printf("Node: \"%s\"\nFull: \"%s\"\nType: 0x%08x\nParent: \"%s\"\n", fn->nodeName, fn->fullName, fn->flags, (fn->parent) ? (fn->parent->nodeName) : ("<NONE>"));
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

static void _FileNodeToMemoryBlock(FileNode_t *fn, MemoryBlock_t *mb)
{
    MemoryBlock_t nodeM, flagsM;
    unsigned char flagsUC[4];

    MWriteString(&nodeM, fn->nodeName);
    flagsM.ptr = flagsUC;
    flagsM.size = sizeof(flagsUC);
    MWriteU32(flagsUC, fn->flags);

    if (fn->flags & _FILENODE_FLAG_IS_DIR)
    {
        MemoryBlock_t countM;
        unsigned char countUC[8];
        MemoryBlock_t *results;
        size_t i;

        countM.ptr = countUC;
        countM.size = sizeof(countUC);
        MWriteU64(countUC, fn->folder.childrenLen);

        results = Mmalloc(sizeof(*results) * (fn->folder.childrenLen + 3));
        memcpy(results + 0, &nodeM, sizeof(*results));
        memcpy(results + 1, &flagsM, sizeof(*results));
        memcpy(results + 2, &countM, sizeof(*results));
        for (i = 0; i < fn->folder.childrenLen; i += 1)
            _FileNodeToMemoryBlock((fn->folder.children)[i], results + i + 3);

        MMConcatA(mb, fn->folder.childrenLen + 3, results);

        for (i = 0; i < fn->folder.childrenLen; i += 1)
            MBfree(results + i + 3);
        Mfree(results);
    }
    else
    {
        MemoryBlock_t sizeM, mtimeM, crc32M;
        unsigned char sizeUC[8], mtimeUC[8], crc32UC[4];

        sizeM.ptr = sizeUC;
        sizeM.size = sizeof(sizeUC);
        MWriteU64(sizeUC, fn->file.size);
        mtimeM.ptr = mtimeUC;
        mtimeM.size = sizeof(mtimeUC);
        MWriteU64(mtimeUC, fn->file.timeLastModification);
        crc32M.ptr = crc32UC;
        crc32M.size = sizeof(crc32UC);
        MWriteU32(crc32UC, fn->file.crc32);

        MMConcat(mb, 5, &nodeM, &flagsM, &sizeM, &mtimeM, &crc32M);
    }
    MBfree(&nodeM);
}

static FileNode_t *_FileNodeFromMemoryBlock(FileNode_t *parent, const char *parentPath, void **ptr, size_t *maxLength)
{
    FileNode_t *fn;
    char *node;
    uint32_t flagsU32;

    node = MReadString(ptr, maxLength);
    if (node == NULL)
        return NULL;

    if ((*maxLength) < sizeof(flagsU32))
    {
        Mfree(node);
        return NULL;
    }

    flagsU32 = MReadU32(ptr);
    (*maxLength) -= sizeof(flagsU32);

    fn = (FileNode_t *)Mmalloc(sizeof(*fn));
    fn->nodeName = node;
    fn->fullName = _PathConcat(parentPath, node);
    fn->parent = parent;
    fn->flags = (unsigned int)flagsU32;

    if (fn->flags & _FILENODE_FLAG_IS_DIR)
    {
        uint64_t countU64;
        FileNode_t *child;
        size_t i;

        if ((*maxLength) < sizeof(countU64))
        {
            Mfree(node);
            Mfree(fn->fullName);
            Mfree(fn);
            return NULL;
        }

        countU64 = MReadU64(ptr);
        (*maxLength) -= sizeof(countU64);

        fn->folder.childrenLen = (size_t)countU64;
        fn->folder.children = (FileNode_t **)Mmalloc(sizeof(*(fn->folder.children)) * fn->folder.childrenLen);
        for (i = 0; i < fn->folder.childrenLen; i += 1)
        {
            child = _FileNodeFromMemoryBlock(fn, fn->fullName, ptr, maxLength);
            if (child == NULL)
            {
                size_t j;
                for (j = 0; j < i; j += 1)
                {
                    _DestoryFileNode((fn->folder.children)[j]);
                    Mfree((fn->folder.children)[j]);
                }
                Mfree(fn->folder.children);
                Mfree(node);
                Mfree(fn->fullName);
                Mfree(fn);
                return NULL;
            }
            (fn->folder.children)[i] = child;
        }
    }
    else
    {
        uint64_t sizeU64, mtimeU64;
        uint32_t crc32U32;

        if ((*maxLength) < sizeof(sizeU64) + sizeof(mtimeU64) + sizeof(crc32U32))
        {
            _DestoryFileNode(fn);
            Mfree(fn);
            return NULL;
        }

        sizeU64 = MReadU64(ptr);
        mtimeU64 = MReadU64(ptr);
        crc32U32 = MReadU32(ptr);
        (*maxLength) -= (sizeof(sizeU64) + sizeof(mtimeU64) + sizeof(crc32U32));

        fn->file.size = (size_t)sizeU64;
        fn->file.timeLastModification = (time_t)mtimeU64;
        fn->file.crc32 = crc32U32;
    }

    return fn;
}

static void _FileTreeConstructAfterLoadingFromMemoryBlock(FileTree_t *t)
{
    TC_t Files, Folders;
    size_t i;

    TCInit(&Files);
    TCInit(&Folders);

    if (t->baseChildren)
        for (i = 0; i < t->baseChildrenLen; i += 1)
            _FileTreeConstructAfterLoadingFromMemoryBlock_Node(t->baseChildren[i], &Files, &Folders);

    TCTransform(&Files);
    TCTransform(&Folders);

    t->totalFilesLen = TCCount(&Files);
    t->totalFiles = (FileNode_t **)Mmalloc(sizeof(*(t->totalFiles)) * t->totalFilesLen);
    memcpy(t->totalFiles, Files.fixedStorage.storage, sizeof(*(t->totalFiles)) * t->totalFilesLen);

    t->totalFoldersLen = TCCount(&Folders);
    t->totalFolders = (FileNode_t **)Mmalloc(sizeof(*(t->totalFolders)) * t->totalFoldersLen);
    memcpy(t->totalFolders, Folders.fixedStorage.storage, sizeof(*(t->totalFolders)) * t->totalFoldersLen);

    TCDeInit(&Files);
    TCDeInit(&Folders);
}

static void _FileTreeConstructAfterLoadingFromMemoryBlock_Node(FileNode_t *fn, TC_t *files, TC_t *folders)
{
    size_t i;

    if (fn->flags & _FILENODE_FLAG_IS_DIR)
    {
        TCAdd(folders, fn);
        for (i = 0; i < fn->folder.childrenLen; i += 1)
            _FileTreeConstructAfterLoadingFromMemoryBlock_Node((fn->folder.children)[i], files, folders);
    }
    else
        TCAdd(files, fn);
}
