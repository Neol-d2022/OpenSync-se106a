#ifndef _FILE_TREE_H_LOADED
#define _FILE_TREE_H_LOADED

/* uint32_t */
#include <stdint.h>

/* size_t */
#include <stddef.h>

/* time_t */
#include <time.h>

/* MemoryBlock_t */
#include "mb.h"

/* TC_t */
#include "transformcontainer.h"

#define FILENODE_FLAG_IS_DIR 0x00000001
#define FILENODE_FLAG_CRC_VALID 0x00000002
#define FILENODE_FLAG_CREATED 0x00000004
#define FILENODE_FLAG_DELETED 0x00000008
#define FILENODE_FLAG_MODIFIED 0x00000010
#define FILENODE_FLAG_MOVED_FROM 0x00000020
#define FILENODE_FLAG_MOVED_TO 0x00000040
#define FILENODE_FLAG_VERSION_VALID 0x00000080

#define FLAG_SET(f, x) ((f) |= (x))
#define FLAG_RESET(f, x) ((f) &= (~(x)))
#define FLAG_ISSET(f, x) ((f) & (x))

typedef struct
{
    size_t size;
    time_t timeLastModification;
    uint32_t crc32;
    uint32_t version;
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
    unsigned int flags;
} FileNode_t;

typedef struct
{
    char *basePath;
    FileNode_t ****indexes;
    FileNode_t **baseChildren;
    FileNode_t **totalFiles;
    FileNode_t **totalFolders;
    size_t baseChildrenLen;
    size_t totalFilesLen;
    size_t totalFoldersLen;
} FileTree_t;

typedef struct
{
    FileNode_t *from;
    FileNode_t *to;
} FileNodeDiff_t;

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

/* File Tree to Memory Block */
void FileTreeToMemoryblock(FileTree_t *t, MemoryBlock_t *mb);

/* Memory Block to File Tree, Return NULL if invalid */
FileTree_t *FileTreeFromMemoryBlock(MemoryBlock_t *mb, const char *parentPath);

/* Compute CRC32 of every files under the tree */
int FileTreeComputeCRC32(FileTree_t *t);

/* Compute Difference */
unsigned int FileTreeDiff(FileTree_t *t_old, FileTree_t *t_new, FileNodeDiff_t ***diff, size_t *diffLen);

/* Release Object */
void FileNodeDiffRelease(FileNodeDiff_t **diff, size_t len);

/* DEBUG. Print file node difference */
void FileNodeDiffDebugPrint(FileNodeDiff_t **diff, size_t len);

FileTree_t *FileTreeFromFile(const char *filename, const char *syncdir);
int FileTreeToFile(const char *filename, FileTree_t *ft);

#endif
