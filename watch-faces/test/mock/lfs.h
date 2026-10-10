#ifndef LFS_H
#define LFS_H

#include <stdint.h>

#define LFS_NAME_MAX 32

enum lfs_type {
    LFS_TYPE_REG = 0x001,
    LFS_TYPE_DIR = 0x002,
};

typedef struct {
    int unused;
} lfs_t;

typedef struct {
    int index;
} lfs_dir_t;

struct lfs_info {
    uint8_t type;
    uint32_t size;
    char name[LFS_NAME_MAX + 1];
};

// Directory listing over the files in the filesystem mock. Only "/" exists.
int lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path);
int lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info);
int lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir);

#endif // LFS_H
