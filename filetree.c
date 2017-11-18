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
#define _FILENODE_FLAG_CREATED 0x00000004
#define _FILENODE_FLAG_DELETED 0x00000008
#define _FILENODE_FLAG_MODIFIED 0x00000010
#define _FILENODE_FLAG_MOVED_FROM 0x00000020
#define _FILENODE_FLAG_MOVED_TO 0x00000040
#define _FILENODE_FLAG_VERSION_VALID 0x00000080

#define _FLAG_SET(f, x) ((f) |= (x))
#define _FLAG_RESET(f, x) ((f) &= (~(x)))
#define _FLAG_ISSET(f, x) ((f) & (x))

#define _FILE_TYPE_UNKNOWN 0
#define _FILE_TYPE_REGULAR 1
#define _FILE_TYPE_FOLDER 2

#define _INDEX_TABLE_FILE 0
#define _INDEX_TABLE_FOLDER 1
#define _INDEX_TABLE_ALL 2
#define _INDEX_TABLES 3

#define _INDEX_FILE_NAME 0
#define _INDEX_FILE_FULLNAME 1
#define _INDEX_FILE_SIZE 2
#define _INDEX_FILE_MTIME 3
#define _INDEX_FILE_CRC32 4
#define _INDEX_FILE_VERSION 5
#define _INDEX_FILE_TRACK 6
#define _INDEX_FILE_NUMBER 7

#define _INDEX_FOLDER_NAME 0
#define _INDEX_FOLDER_FULLNAME 1
#define _INDEX_FOLDER_NUMBER 2

#define _INDEX_ALL_NAME 0
#define _INDEX_ALL_FULLNAME 1
#define _INDEX_ALL_NUMBER 2

static const size_t _INDEX_TABLE_LENGTHS[_INDEX_TABLES] = {_INDEX_FILE_NUMBER, _INDEX_FOLDER_NUMBER, _INDEX_ALL_NUMBER};

static int _FileTreeScanRecursive(const char *fullPath, TC_t *FNs, TC_t *FNFiles, TC_t *FNFolders);
static char *_PathConcat(const char *parent, const char *filename);
static int _IsPathSeparator(const char *c);
static int _GetFileStat(const char *fullPath, int *result, FileNodeTypeFile_t *fnfile);
static void _DestoryFileNode(FileNode_t *fn, void *param);
static void _PrintFileNode(FileNode_t *fn, void *param);
static void _FileNodeToMemoryBlock(FileNode_t *fn, MemoryBlock_t *mb);
static FileNode_t *_FileNodeFromMemoryBlock(FileNode_t *parent, const char *parentPath, void **ptr, size_t *maxLength);
static void _FileTreeConstructAfterLoadingFromMemoryBlock(FileTree_t *t);
static void _FileTreeConstructAfterLoadingFromMemoryBlock_Node(FileNode_t *fn, void *param);
static size_t _DuplicateStorageFromTCTransformed(FileNode_t ***base, TC_t *tc);
static void _AutoVariableToMemoryBlock(MemoryBlock_t *mb, void *ptr, size_t size);
static void _FileNodeTraverse(FileNode_t *fn, void *param, void (*traverser)(FileNode_t *fn, void *param));
static void _FileTreeReleaseIndex(FileTree_t *t);
static void _FileTreeRefreshIndex(FileTree_t *t);
static void _FileTreeDiff_SetFlag(FileNode_t *fn, void *param);
static int _FileNodeIsVersionChanged(FileNode_t *fn);

#define _INTEGER_CMP(a, b) (((a) > (b)) ? (1) : (((a) < (b)) ? (-1) : 0))
static int _FileNodeCmp_String(const void *a, const void *b);
static int _FileNodeCmp_UInt32(const void *a, const void *b);
static int _FileNodeCmp_UInt64(const void *a, const void *b);

static int _FileNodeCmp_File_Name(const void *a, const void *b);
static int _FileNodeCmp_File_FullName(const void *a, const void *b);
static int _FileNodeCmp_File_FileSize(const void *a, const void *b);
static int _FileNodeCmp_File_MTime(const void *a, const void *b);
static int _FileNodeCmp_File_CRC32(const void *a, const void *b);
static int _FileNodeCmp_File_Version(const void *a, const void *b);
static int _FileNodeCmp_File_Track(const void *a, const void *b);

static int (*_FileNodeCmp_File_indexFunctions[_INDEX_FILE_NUMBER])(const void *, const void *) = {
    _FileNodeCmp_File_Name,
    _FileNodeCmp_File_FullName,
    _FileNodeCmp_File_FileSize,
    _FileNodeCmp_File_MTime,
    _FileNodeCmp_File_CRC32,
    _FileNodeCmp_File_Version,
    _FileNodeCmp_File_Track};

static int _FileNodeCmp_Folder_Name(const void *a, const void *b);
static int _FileNodeCmp_Folder_FullName(const void *a, const void *b);

static int (*_FileNodeCmp_Folder_indexFunctions[_INDEX_FOLDER_NUMBER])(const void *, const void *) = {
    _FileNodeCmp_Folder_Name,
    _FileNodeCmp_Folder_FullName};

static int _FileNodeCmp_All_Name(const void *a, const void *b);
static int _FileNodeCmp_All_FullName(const void *a, const void *b);

static int (*_FileNodeCmp_All_indexFunctions[_INDEX_ALL_NUMBER])(const void *, const void *) = {
    _FileNodeCmp_All_Name,
    _FileNodeCmp_All_FullName};

static int (**_FileNodeCmp_indexTables[_INDEX_TABLES])(const void *, const void *) = {
    _FileNodeCmp_File_indexFunctions,
    _FileNodeCmp_Folder_indexFunctions,
    _FileNodeCmp_All_indexFunctions};

typedef struct
{
    TC_t *files;
    TC_t *folders;
} Reconstruct_internal_object_t;

typedef struct
{
    unsigned int mask;
    unsigned char mode;
} FileTreeDiff_SetFlag_internal_object_t;

void FileTreeInit(FileTree_t *t)
{
    memset(t, 0, sizeof(*t));
    t->basePath = SDup(".");
}

void FileTreeDeInit(FileTree_t *t)
{
    size_t i;

    _FileTreeReleaseIndex(t);
    Mfree(t->basePath);
    if (t->baseChildren)
    {
        for (i = 0; i < t->baseChildrenLen; i += 1)
        {
            _FileNodeTraverse(t->baseChildren[i], NULL, _DestoryFileNode);
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

    t->baseChildrenLen = _DuplicateStorageFromTCTransformed(&(t->baseChildren), &FNs);
    t->totalFilesLen = _DuplicateStorageFromTCTransformed(&(t->totalFiles), &Files);
    t->totalFoldersLen = _DuplicateStorageFromTCTransformed(&(t->totalFolders), &Folders);

    TCDeInit(&FNs);
    TCDeInit(&Files);
    TCDeInit(&Folders);

    _FileTreeRefreshIndex(t);

    return r;
}

void FileTreeDebugPrint(FileTree_t *t)
{
    size_t i, depth = 0;

    printf("Path = \"%s\"\nTotal Files = %u\nTotal Folders = %u\n\n", t->basePath, (unsigned int)t->totalFilesLen, (unsigned int)t->totalFoldersLen);
    if (t->baseChildren)
        for (i = 0; i < t->baseChildrenLen; i += 1)
            _FileNodeTraverse(t->baseChildren[i], &depth, _PrintFileNode);
}

void FileTreeToMemoryblock(FileTree_t *t, MemoryBlock_t *mb)
{
    MemoryBlock_t baseCountM;
    unsigned char baseCountUC[8];
    MemoryBlock_t *results;
    size_t i;

    MWriteU64(baseCountUC, t->baseChildrenLen);
    _AutoVariableToMemoryBlock(&baseCountM, baseCountUC, sizeof(baseCountUC));

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
                _FileNodeTraverse((t->baseChildren)[j], NULL, _DestoryFileNode);
                Mfree((t->baseChildren)[j]);
            }
            Mfree(t->baseChildren);
            Mfree(t->basePath);
            Mfree(t);
            return NULL;
        }
        (t->baseChildren)[i] = child;
    }

    t->indexes = NULL;
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
                _FLAG_SET((t->totalFiles)[i]->flags, _FILENODE_FLAG_CRC_VALID);
            }
            fclose(f);
        }
        else
        {
            _FLAG_RESET((t->totalFiles)[i]->flags, _FILENODE_FLAG_CRC_VALID);
            r = errno;
        }
    }

    return r;
}

unsigned int FileTreeDiff(FileTree_t *t_old, FileTree_t *t_new)
{
    size_t IndexLength[2][_INDEX_TABLES] = {
        {t_old->totalFilesLen,
         t_old->totalFoldersLen,
         t_old->totalFilesLen + t_old->totalFoldersLen},
        {t_new->totalFilesLen,
         t_new->totalFoldersLen,
         t_new->totalFilesLen + t_new->totalFoldersLen}};
    unsigned char *checked[4];
    FileTreeDiff_SetFlag_internal_object_t sfio;
    size_t i, n, idx;
    FileNode_t *fn1, *fn2, **_fn;
    unsigned int diffCount = 0;

    checked[0] = (unsigned char *)Mmalloc(t_old->totalFoldersLen);
    checked[1] = (unsigned char *)Mmalloc(t_new->totalFoldersLen);
    checked[2] = (unsigned char *)Mmalloc(t_old->totalFilesLen);
    checked[3] = (unsigned char *)Mmalloc(t_new->totalFilesLen);

    memset(checked[0], 0, t_old->totalFoldersLen);
    memset(checked[1], 0, t_new->totalFoldersLen);
    memset(checked[2], 0, t_old->totalFilesLen);
    memset(checked[3], 0, t_new->totalFilesLen);

    n = t_old->totalFoldersLen;
    for (i = 0; i < n; i += 1)
    {
        fn1 = t_old->totalFolders[i];
        _fn = (FileNode_t **)bsearch(&fn1, t_new->indexes[_INDEX_TABLE_FOLDER][_INDEX_FOLDER_FULLNAME], IndexLength[1][_INDEX_TABLE_FOLDER], sizeof(***(t_new->indexes)), _FileNodeCmp_indexTables[_INDEX_TABLE_FOLDER][_INDEX_FOLDER_FULLNAME]);
        if (_fn)
        {
            /* We found the corresponding folder in the new tree */
            idx = ((size_t)_fn - (size_t)(t_new->indexes[_INDEX_TABLE_FOLDER][_INDEX_FOLDER_FULLNAME])) / sizeof(*_fn);
            checked[0][i] = checked[1][idx] = 1;
        }
        else
        {
            /* We cannot found the corresponding folder in new tree, assuming the folder was deleted */
            sfio.mask = _FILENODE_FLAG_DELETED;
            sfio.mode = 1;
            _FileNodeTraverse(fn1, &sfio, _FileTreeDiff_SetFlag);
            checked[0][i] = 1;
            diffCount += 1;
        }
    }

    n = t_new->totalFoldersLen;
    for (i = 0; i < n; i += 1)
    {
        if (!(checked[1][i]))
        {
            /* The folder has not been searched. Must be newly created */
            fn2 = t_new->indexes[_INDEX_TABLE_FOLDER][_INDEX_FOLDER_FULLNAME][i];
            sfio.mask = _FILENODE_FLAG_CREATED;
            sfio.mode = 1;
            _FileNodeTraverse(fn2, &sfio, _FileTreeDiff_SetFlag);
            checked[1][i] = 1;
            diffCount += 1;
        }
    }

    n = t_old->totalFilesLen;
    for (i = 0; i < n; i += 1)
    {
        fn1 = t_old->totalFiles[i];

        /* Check the full path */
        _fn = (FileNode_t **)bsearch(&fn1, t_new->indexes[_INDEX_TABLE_FILE][_INDEX_FILE_FULLNAME], IndexLength[1][_INDEX_TABLE_FILE], sizeof(***(t_new->indexes)), _FileNodeCmp_indexTables[_INDEX_TABLE_FILE][_INDEX_FILE_FULLNAME]);

        if (_fn)
        {
            /* File exist in the new tree, check if it has been modified */

            fn2 = *_fn;
            idx = ((size_t)_fn - (size_t)(t_new->indexes[_INDEX_TABLE_FILE][_INDEX_FILE_FULLNAME])) / sizeof(*_fn);

            if (!_FLAG_ISSET(fn2->flags, _FILENODE_FLAG_CRC_VALID))
            {
                /* We cannot tell if it has been modified */
                abort();
            }
            else if (_FileNodeCmp_indexTables[_INDEX_TABLE_FILE][_INDEX_FILE_TRACK](&fn1, &fn2))
            {
                /* Content has been modified */
                _FLAG_SET(fn1->flags, _FILENODE_FLAG_MODIFIED);
                checked[2][i] = checked[3][idx] = 1;
                diffCount += 1;
            }
            else
            {
                /* Nothing changed */
                checked[2][i] = checked[3][idx] = 1;
            }
        }
        else
        {
            /* We found nothing. The file may be deleted or moved. Assuming the file has been removed. */
            if (!_FLAG_ISSET(fn1->flags, _FILENODE_FLAG_DELETED))
                _FLAG_SET(fn1->flags, _FILENODE_FLAG_DELETED);
            checked[2][i] = 1;
            diffCount += 1;
        }
    }

    n = t_new->totalFilesLen;
    for (i = 0; i < n; i += 1)
    {
        if (!checked[3][i])
        {
            /* The file has not been searched. Must be newly created */
            fn2 = t_new->indexes[_INDEX_TABLE_FILE][_INDEX_FILE_FULLNAME][i];
            _FLAG_SET(fn2->flags, _FILENODE_FLAG_CREATED);
            diffCount += 1;
        }
    }

    Mfree(checked[0]);
    Mfree(checked[1]);
    Mfree(checked[2]);
    Mfree(checked[3]);

    n = t_old->totalFilesLen;
    for (i = 0; i < n; i += 1)
    {
        fn1 = t_old->totalFiles[i];
        if (_FileNodeIsVersionChanged(fn1))
            _PrintFileNode(fn1, NULL);
    }

    n = t_old->totalFoldersLen;
    for (i = 0; i < n; i += 1)
    {
        fn1 = t_old->totalFolders[i];
        if (_FileNodeIsVersionChanged(fn1))
            _PrintFileNode(fn1, NULL);
    }

    n = t_new->totalFilesLen;
    for (i = 0; i < n; i += 1)
    {
        fn2 = t_new->totalFiles[i];
        if (_FileNodeIsVersionChanged(fn2))
            _PrintFileNode(fn2, NULL);
    }

    n = t_new->totalFoldersLen;
    for (i = 0; i < n; i += 1)
    {
        fn2 = t_new->totalFolders[i];
        if (_FileNodeIsVersionChanged(fn2))
            _PrintFileNode(fn2, NULL);
    }

    return diffCount;
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
                            _FLAG_RESET(fn->flags, _FILENODE_FLAG_CRC_VALID);
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
                        _FLAG_SET(fn->flags, _FILENODE_FLAG_IS_DIR);
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
        fn->folder.childrenLen = _DuplicateStorageFromTCTransformed(&(fn->folder.children), &SubDIR);
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

static void _DestoryFileNode(FileNode_t *fn, void *param)
{
    size_t i;

    param = param;
    Mfree(fn->nodeName);
    Mfree(fn->fullName);
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
    {
        if (fn->folder.children)
        {
            for (i = 0; i < fn->folder.childrenLen; i += 1)
                Mfree((fn->folder.children)[i]);
            Mfree(fn->folder.children);
        }
    }
}

static void _PrintFileNode(FileNode_t *fn, void *param)
{
    char buf[64];
    struct tm t;

    param = param;
    printf("Node: \"%s\"\nFull: \"%s\"\nFlag: 0x%08x\nParent: \"%s\"\n", fn->nodeName, fn->fullName, fn->flags, (fn->parent) ? (fn->parent->nodeName) : ("<NONE>"));
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_CRC_VALID))
        printf("File CRC32 is valid.\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_CREATED))
        printf("File/Folder is newly created (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_DELETED))
        printf("File/Folder is deleted (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_MODIFIED))
        printf("File/Folder is modified (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_MOVED_FROM))
        printf("File/Folder is moved from here (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_MOVED_TO))
        printf("File/Folder is moved to here (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_VERSION_VALID))
        printf("File/Folder version is valid (Diff).\n");
    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
    {
        printf("It is a folder.\n");
        printf("Children count: %u\n\n", (unsigned int)fn->folder.childrenLen);
    }
    else
    {
        t = *localtime(&(fn->file.timeLastModification));
        strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", &t);
        printf("CRC32: 0x%08X\nMTime: %s\nSize: %u\n\n", fn->file.crc32, buf, (unsigned int)fn->file.size);
    }
}

static void _FileNodeToMemoryBlock(FileNode_t *fn, MemoryBlock_t *mb)
{
    MemoryBlock_t nodeM, flagsM;
    unsigned char flagsUC[4];

    MWriteString(&nodeM, fn->nodeName);
    MWriteU32(flagsUC, fn->flags);
    _AutoVariableToMemoryBlock(&flagsM, flagsUC, sizeof(flagsUC));

    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
    {
        MemoryBlock_t countM;
        unsigned char countUC[8];
        MemoryBlock_t *results;
        size_t i;

        MWriteU64(countUC, fn->folder.childrenLen);
        _AutoVariableToMemoryBlock(&countM, countUC, sizeof(countUC));

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
        MemoryBlock_t sizeM, mtimeM, crc32M, verM;
        unsigned char sizeUC[8], mtimeUC[8], crc32UC[4], verUC[4];

        MWriteU64(sizeUC, fn->file.size);
        _AutoVariableToMemoryBlock(&sizeM, sizeUC, sizeof(sizeUC));
        MWriteU64(mtimeUC, fn->file.timeLastModification);
        _AutoVariableToMemoryBlock(&mtimeM, mtimeUC, sizeof(mtimeUC));
        MWriteU32(crc32UC, fn->file.crc32);
        _AutoVariableToMemoryBlock(&crc32M, crc32UC, sizeof(crc32UC));
        MWriteU32(verUC, fn->file.version);
        _AutoVariableToMemoryBlock(&verM, verUC, sizeof(verUC));

        MMConcat(mb, 6, &nodeM, &flagsM, &sizeM, &mtimeM, &crc32M, &verM);
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

    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
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
                fn->folder.childrenLen = i;
                _FileNodeTraverse(fn, NULL, _DestoryFileNode);
                return NULL;
            }
            (fn->folder.children)[i] = child;
        }
    }
    else
    {
        uint64_t sizeU64, mtimeU64;
        uint32_t crc32U32, verU32;

        if ((*maxLength) < sizeof(sizeU64) + sizeof(mtimeU64) + sizeof(crc32U32) + sizeof(verU32))
        {
            _FileNodeTraverse(fn, NULL, _DestoryFileNode);
            Mfree(fn);
            return NULL;
        }

        sizeU64 = MReadU64(ptr);
        mtimeU64 = MReadU64(ptr);
        crc32U32 = MReadU32(ptr);
        verU32 = MReadU32(ptr);
        (*maxLength) -= (sizeof(sizeU64) + sizeof(mtimeU64) + sizeof(crc32U32) + sizeof(verU32));

        fn->file.size = (size_t)sizeU64;
        fn->file.timeLastModification = (time_t)mtimeU64;
        fn->file.crc32 = crc32U32;
        fn->file.version = verU32;
    }

    return fn;
}

static void _FileTreeConstructAfterLoadingFromMemoryBlock(FileTree_t *t)
{
    Reconstruct_internal_object_t io;
    TC_t Files, Folders;
    size_t i;

    TCInit(&Files);
    TCInit(&Folders);

    io.files = &Files;
    io.folders = &Folders;

    if (t->baseChildren)
        for (i = 0; i < t->baseChildrenLen; i += 1)
            _FileNodeTraverse(t->baseChildren[i], &io, _FileTreeConstructAfterLoadingFromMemoryBlock_Node);

    TCTransform(&Files);
    TCTransform(&Folders);

    t->totalFilesLen = _DuplicateStorageFromTCTransformed(&(t->totalFiles), &Files);
    t->totalFoldersLen = _DuplicateStorageFromTCTransformed(&(t->totalFolders), &Folders);

    TCDeInit(&Files);
    TCDeInit(&Folders);

    _FileTreeRefreshIndex(t);
}

static void _FileTreeConstructAfterLoadingFromMemoryBlock_Node(FileNode_t *fn, void *param)
{
    Reconstruct_internal_object_t *io = (Reconstruct_internal_object_t *)param;

    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
        TCAdd(io->folders, fn);
    else
        TCAdd(io->files, fn);
}

static size_t _DuplicateStorageFromTCTransformed(FileNode_t ***base, TC_t *tc)
{
    size_t count;

    count = TCCount(tc);
    *base = (FileNode_t **)Mmalloc(sizeof(**base) * count);
    memcpy(*base, tc->fixedStorage.storage, sizeof(**base) * count);

    return count;
}

static void _AutoVariableToMemoryBlock(MemoryBlock_t *mb, void *ptr, size_t size)
{
    mb->ptr = ptr;
    mb->size = size;
}

static void _FileNodeTraverse(FileNode_t *fn, void *param, void (*traverser)(FileNode_t *fn, void *param))
{
    size_t i;

    if (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_IS_DIR))
        for (i = 0; i < fn->folder.childrenLen; i += 1)
            _FileNodeTraverse((fn->folder.children)[i], param, traverser);

    traverser(fn, param);
}

static int _FileNodeCmp_String(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static int _FileNodeCmp_UInt32(const void *a, const void *b)
{
    uint32_t c = *(uint32_t *)a;
    uint32_t d = *(uint32_t *)b;

    return _INTEGER_CMP(c, d);
}

static int _FileNodeCmp_UInt64(const void *a, const void *b)
{
    uint64_t c = *(uint64_t *)a;
    uint64_t d = *(uint64_t *)b;

    return _INTEGER_CMP(c, d);
}

static int _FileNodeCmp_All_Name(const void *a, const void *b)
{
    return _FileNodeCmp_String((*(FileNode_t **)a)->nodeName, (*(FileNode_t **)b)->nodeName);
}

static int _FileNodeCmp_All_FullName(const void *a, const void *b)
{
    return _FileNodeCmp_String((*(FileNode_t **)a)->fullName, (*(FileNode_t **)b)->fullName);
}

static int _FileNodeCmp_File_Name(const void *a, const void *b)
{
    return _FileNodeCmp_All_Name(a, b);
}

static int _FileNodeCmp_File_FullName(const void *a, const void *b)
{
    return _FileNodeCmp_All_FullName(a, b);
}

static int _FileNodeCmp_File_FileSize(const void *a, const void *b)
{
    uint64_t c = (*(FileNode_t **)a)->file.size, d = (*(FileNode_t **)b)->file.size;
    return _FileNodeCmp_UInt64(&c, &d);
}

static int _FileNodeCmp_File_MTime(const void *a, const void *b)
{
    uint64_t c = (*(FileNode_t **)a)->file.timeLastModification, d = (*(FileNode_t **)b)->file.timeLastModification;
    return _FileNodeCmp_UInt64(&c, &d);
}

static int _FileNodeCmp_File_CRC32(const void *a, const void *b)
{
    return _FileNodeCmp_UInt32(&((*(FileNode_t **)a)->file.crc32), &((*(FileNode_t **)b)->file.crc32));
}

static int _FileNodeCmp_File_Version(const void *a, const void *b)
{
    return _FileNodeCmp_UInt32(&((*(FileNode_t **)a)->file.version), &((*(FileNode_t **)b)->file.version));
}

static int _FileNodeCmp_Folder_Name(const void *a, const void *b)
{
    return _FileNodeCmp_All_Name(a, b);
}

static int _FileNodeCmp_Folder_FullName(const void *a, const void *b)
{
    return _FileNodeCmp_All_FullName(a, b);
}

static int _FileNodeCmp_File_Track(const void *a, const void *b)
{
    int c;

    c = _FileNodeCmp_File_CRC32(a, b);
    if (c)
        return c;
    else
        return _FileNodeCmp_File_FileSize(a, b);
}

static void _FileTreeReleaseIndex(FileTree_t *t)
{
    size_t i, j, n;

    if (t->indexes)
    {
        for (i = 0; i < _INDEX_TABLES; i += 1)
        {
            n = _INDEX_TABLE_LENGTHS[i];
            for (j = 0; j < n; j += 1)
            {
                Mfree(t->indexes[i][j]);
            }
            Mfree(t->indexes[i]);
        }
        Mfree(t->indexes);
    }
}

static void _FileTreeRefreshIndex(FileTree_t *t)
{
    size_t IndexLength[_INDEX_TABLES] = {t->totalFilesLen, t->totalFoldersLen, t->totalFilesLen + t->totalFoldersLen};
    FileNode_t **IndexSource[_INDEX_TABLES] = {t->totalFiles, t->totalFolders, NULL};
    FileNode_t **IndexSourceAll;
    size_t i, j, n;

    _FileTreeReleaseIndex(t);

    IndexSourceAll = (FileNode_t **)Mmalloc(sizeof(*IndexSourceAll) * IndexLength[_INDEX_TABLE_ALL]);
    memcpy(IndexSourceAll, IndexSource[_INDEX_TABLE_FILE], sizeof(*IndexSourceAll) * IndexLength[_INDEX_TABLE_FILE]);
    memcpy(IndexSourceAll + IndexLength[_INDEX_TABLE_FILE], IndexSource[_INDEX_TABLE_FOLDER], sizeof(*IndexSourceAll) * IndexLength[_INDEX_TABLE_FOLDER]);
    IndexSource[_INDEX_TABLE_ALL] = IndexSourceAll;

    t->indexes = (FileNode_t ****)Mmalloc(sizeof(*(t->indexes)) * _INDEX_TABLES);
    for (i = 0; i < _INDEX_TABLES; i += 1)
    {
        n = _INDEX_TABLE_LENGTHS[i];
        t->indexes[i] = (FileNode_t ***)Mmalloc(sizeof(**(t->indexes)) * n);
        for (j = 0; j < n; j += 1)
        {
            t->indexes[i][j] = (FileNode_t **)Mmalloc(sizeof(***(t->indexes)) * IndexLength[i]);
            memcpy(t->indexes[i][j], IndexSource[i], sizeof(***(t->indexes)) * IndexLength[i]);
            qsort(t->indexes[i][j], IndexLength[i], sizeof(***(t->indexes)), _FileNodeCmp_indexTables[i][j]);
        }
    }

    Mfree(IndexSourceAll);
}

static void _FileTreeDiff_SetFlag(FileNode_t *fn, void *param)
{
    FileTreeDiff_SetFlag_internal_object_t *io = (FileTreeDiff_SetFlag_internal_object_t *)param;

    if (io->mode)
        _FLAG_SET(fn->flags, io->mask);
    else
        _FLAG_RESET(fn->flags, io->mask);
}

static int _FileNodeIsVersionChanged(FileNode_t *fn)
{
    return (_FLAG_ISSET(fn->flags, _FILENODE_FLAG_CREATED) || _FLAG_ISSET(fn->flags, _FILENODE_FLAG_DELETED) || _FLAG_ISSET(fn->flags, _FILENODE_FLAG_MODIFIED) || _FLAG_ISSET(fn->flags, _FILENODE_FLAG_MOVED_FROM) || _FLAG_ISSET(fn->flags, _FILENODE_FLAG_MOVED_TO)) ? (1) : (0);
}
