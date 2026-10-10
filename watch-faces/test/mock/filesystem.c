#include "filesystem.h"
#include "lfs.h"

#include <stddef.h>
#include <string.h>

#define MOCK_MAX_FILES 8
#define MOCK_MAX_FILE_SIZE 256

typedef struct {
    bool used;
    char name[LFS_NAME_MAX + 1];
    char data[MOCK_MAX_FILE_SIZE + 1];
    int32_t size;
} mock_file_t;

static mock_file_t files[MOCK_MAX_FILES];

lfs_t eeprom_filesystem;

static mock_file_t *find_file(const char *filename) {
    for (int i = 0; i < MOCK_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, filename) == 0) return &files[i];
    }
    return NULL;
}

static mock_file_t *find_or_create_file(const char *filename) {
    mock_file_t *file = find_file(filename);
    if (file) return file;
    if (strlen(filename) > LFS_NAME_MAX) return NULL;
    for (int i = 0; i < MOCK_MAX_FILES; i++) {
        if (!files[i].used) {
            memset(&files[i], 0, sizeof(files[i]));
            files[i].used = true;
            strcpy(files[i].name, filename);
            return &files[i];
        }
    }
    return NULL;
}

bool filesystem_write_file(char *filename, char *text, int32_t length) {
    mock_file_t *file = find_or_create_file(filename);
    if (!file || length > MOCK_MAX_FILE_SIZE) return false;
    memcpy(file->data, text, length);
    file->size = length;
    file->data[length] = '\0';
    return true;
}

bool filesystem_append_file(char *filename, char *text, int32_t length) {
    mock_file_t *file = find_or_create_file(filename);
    if (!file || file->size + length > MOCK_MAX_FILE_SIZE) return false;
    memcpy(file->data + file->size, text, length);
    file->size += length;
    file->data[file->size] = '\0';
    return true;
}

void filesystem_mock_reset(void) {
    memset(files, 0, sizeof(files));
}

const char *filesystem_mock_get_file(const char *filename) {
    mock_file_t *file = find_file(filename);
    return file ? file->data : NULL;
}

int lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path) {
    (void)lfs;
    if (strcmp(path, "/") != 0) return -1;
    dir->index = 0;
    return 0;
}

int lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info) {
    (void)lfs;
    while (dir->index < MOCK_MAX_FILES) {
        mock_file_t *file = &files[dir->index++];
        if (!file->used) continue;
        info->type = LFS_TYPE_REG;
        info->size = file->size;
        strcpy(info->name, file->name);
        return 1;
    }
    return 0;
}

int lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir) {
    (void)lfs;
    (void)dir;
    return 0;
}
