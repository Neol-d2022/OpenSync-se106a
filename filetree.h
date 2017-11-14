#ifndef _FILE_TREE_H_LOADED
#define _FILE_TREE_H_LOADED

/* uint32_t */
#include <stdint.h>

/* size_t */
#include <stddef.h>

typedef struct
{
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
    unsigned char flags;
} FileNode_t;

typedef struct
{
    char *basePath;
    FileNode_t **baseChildren;
    size_t baseChildrenLen;
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
