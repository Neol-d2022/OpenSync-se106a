#ifndef _FILE_TREE_H_LOADED
#define _FILE_TREE_H_LOADED

/* uint32_t */
#include <stdint.h>

/* size_t */
#include <stddef.h>

/* time_t */
#include <time.h>

typedef struct
{
    size_t size;
    time_t timeLastModification;
    uint32_t crc32;
} FileNodeTypeFile_t;

typedef struct
{
    struct FileNode_struct_t **children;
    size_t childrenLen;
} FileNodeTypeFolder_t;

typedef struct FileNode_struct_t
{
    union {
        FileNodeTypeFile_t file;
        FileNodeTypeFolder_t folder;
    };
    char *nodeName;
    char *fullName;
    struct FileNode_struct_t *parent;
    unsigned char flags;
} FileNode_t;

typedef struct
{
    char *basePath;
    FileNode_t **baseChildren;
    FileNode_t **totalFiles;
    FileNode_t **totalFolders;
    size_t baseChildrenLen;
    size_t totalFilesLen;
    size_t totalFoldersLen;
} FileTree_t;

/* Initialize a file tree */
void FileTreeInit(FileTree_t *t);

/* Destroy a file tree */
void FileTreeDeInit(FileTree_t *t);

/* Set Base Path */
void FileTreeSetBasePath(FileTree_t *t, const char *basePath);

/* Scan And Create File Tree*/
int FileTreeScan(FileTree_t *t);

/* DEBUG. Print file tree */
void FileTreeDebugPrint(FileTree_t *t);

#endif