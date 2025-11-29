/*
 * MIT License
 *
 * Copyright (c) 2022-2024 Joey Castillo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesystem.h"
#include "watch.h"
#include "lfs.h"
#include "base64.h"
#include "delay.h"

#ifndef min
#define min(x, y) ((x) > (y) ? (y) : (x))
#endif

int lfs_storage_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int lfs_storage_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int lfs_storage_erase(const struct lfs_config *cfg, lfs_block_t block);
int lfs_storage_sync(const struct lfs_config *cfg);

int lfs_storage_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    (void) cfg;
    return !watch_storage_read(block, off, (void *)buffer, size);
}

int lfs_storage_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    (void) cfg;
    return !watch_storage_write(block, off, (void *)buffer, size);
}

int lfs_storage_erase(const struct lfs_config *cfg, lfs_block_t block) {
    (void) cfg;
    return !watch_storage_erase(block);
}

int lfs_storage_sync(const struct lfs_config *cfg) {
    (void) cfg;
    return !watch_storage_sync();
}

const struct lfs_config watch_lfs_cfg = {
    // block device operations
    .read  = lfs_storage_read,
    .prog  = lfs_storage_prog,
    .erase = lfs_storage_erase,
    .sync  = lfs_storage_sync,

    // block device configuration
    .read_size = 16,
    .prog_size = NVMCTRL_PAGE_SIZE,
    .block_size = NVMCTRL_ROW_SIZE,
    .block_count = NVMCTRL_RWWEE_PAGES / 4,
    .cache_size = NVMCTRL_PAGE_SIZE,
    .lookahead_size = 16,
    .block_cycles = 100,
};

lfs_t eeprom_filesystem;
static lfs_file_t file;
static struct lfs_info info;

static int _traverse_df_cb(void *p, lfs_block_t block) {
    (void) block;
	uint32_t *nb = p;
	*nb += 1;
	return 0;
}

int32_t filesystem_get_free_space(void) {
	int err;

	uint32_t free_blocks = 0;
	err = lfs_fs_traverse(&eeprom_filesystem, _traverse_df_cb, &free_blocks);
	if(err < 0){
		return err;
	}

	uint32_t available = watch_lfs_cfg.block_count * watch_lfs_cfg.block_size - free_blocks * watch_lfs_cfg.block_size;

	return (int32_t)available;
}

int filesystem_get_ls_entries(const char *path, filesystem_ls_callback_t callback, void *user_data) {
    lfs_dir_t dir;
    int err = lfs_dir_open(&eeprom_filesystem, &dir, path);
    if (err < 0) {
        return err;
    }

    struct lfs_info info;
    while (true) {
        int res = lfs_dir_read(&eeprom_filesystem, &dir, &info);
        if (res < 0) {
            lfs_dir_close(&eeprom_filesystem, &dir);
            return res;
        }

        if (res == 0) {
            break;
        }

        const char *type;
        switch (info.type) {
            case LFS_TYPE_REG: type = "file"; break;
            case LFS_TYPE_DIR: type = "dir"; break;
            default:           type = "?"; break;
        }

        if (callback != NULL) {
            callback(type, info.size, info.name, user_data);
        }
    }

    err = lfs_dir_close(&eeprom_filesystem, &dir);
    if (err < 0) {
        return err;
    }

    return 0;
}

static void _ls_print_callback(const char *type, int32_t size, const char *name, void *user_data) {
    (void) user_data;
    printf("%-4s %4ld bytes %s\r\n", type, size, name);
}

static int filesystem_ls(lfs_t *lfs, const char *path) {
    (void) lfs;
    return filesystem_get_ls_entries(path, _ls_print_callback, NULL);
}

bool filesystem_init(void) {
    int err = lfs_mount(&eeprom_filesystem, &watch_lfs_cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err < 0) {
        printf("Ignore that error! Formatting filesystem...\r\n");
        err = lfs_format(&eeprom_filesystem, &watch_lfs_cfg);
        if (err < 0) return false;
        err = lfs_mount(&eeprom_filesystem, &watch_lfs_cfg) == LFS_ERR_OK;
        printf("Filesystem mounted with %ld bytes free.\r\n", filesystem_get_free_space());
    }

    return err == LFS_ERR_OK;
}

int _filesystem_format(void);
int _filesystem_format(void) {
    int err = lfs_unmount(&eeprom_filesystem);
    if (err < 0) {
        printf("Couldn't unmount - continuing to format, but you should reboot afterwards!\r\n");
    }

    err = lfs_format(&eeprom_filesystem, &watch_lfs_cfg);
    if (err < 0) return err;

    err = lfs_mount(&eeprom_filesystem, &watch_lfs_cfg);
    if (err < 0) return err;
    printf("Filesystem re-mounted with %ld bytes free.\r\n", filesystem_get_free_space());
    return 0;
}

bool filesystem_file_exists(char *filename) {
    info.type = 0;
    lfs_stat(&eeprom_filesystem, filename, &info);
    return info.type == LFS_TYPE_REG;
}

bool filesystem_rm(char *filename) {
    info.type = 0;
    lfs_stat(&eeprom_filesystem, filename, &info);
    if (filesystem_file_exists(filename)) {
        return lfs_remove(&eeprom_filesystem, filename) == LFS_ERR_OK;
    } else {
        printf("rm: %s: No such file\r\n", filename);
        return false;
    }
}

int32_t filesystem_get_file_size(char *filename) {
    if (filesystem_file_exists(filename)) {
        return info.size; // info struct was just populated by filesystem_file_exists
    }

    return -1;
}

bool filesystem_read_file(char *filename, char *buf, int32_t length) {
    memset(buf, 0, length);
    int32_t file_size = filesystem_get_file_size(filename);
    if (file_size > 0) {
        int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDONLY);
        if (err < 0) return false;
        err = lfs_file_read(&eeprom_filesystem, &file, buf, min(length, file_size));
        if (err < 0) return false;
        return lfs_file_close(&eeprom_filesystem, &file) == LFS_ERR_OK;
    }

    return false;
}

bool filesystem_read_line(char *filename, char *buf, int32_t *offset, int32_t length) {
    memset(buf, 0, length + 1);
    int32_t file_size = filesystem_get_file_size(filename);
    if (file_size > 0) {
        int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDONLY);
        if (err < 0) return false;
        err = lfs_file_seek(&eeprom_filesystem, &file, *offset, LFS_SEEK_SET);
        if (err < 0) return false;
        err = lfs_file_read(&eeprom_filesystem, &file, buf, min(length - 1, file_size - *offset));
        if (err < 0) return false;
        for(int i = 0; i < length; i++) {
            (*offset)++;
            if (buf[i] == '\n') {
                buf[i] = 0;
                break;
            }
        }
        return lfs_file_close(&eeprom_filesystem, &file) == LFS_ERR_OK;
    }

    return false;
}

char* filesystem_get_cat_output(char *filename) {
    info.type = 0;
    lfs_stat(&eeprom_filesystem, filename, &info);
    if (filesystem_file_exists(filename)) {
        if (info.size > 0) {
            char *buf = malloc(info.size + 1);
            if (buf == NULL) {
                return NULL;
            }
            if (filesystem_read_file(filename, buf, info.size)) {
                buf[info.size] = '\0';
                return buf;
            }
            free(buf);
            return NULL;
        } else {
            // Empty file - return empty string
            char *buf = malloc(1);
            if (buf != NULL) {
                buf[0] = '\0';
            }
            return buf;
        }
    }
    return NULL;
}

static void filesystem_cat(char *filename) {
    char *output = filesystem_get_cat_output(filename);
    if (output != NULL) {
        printf("%s\r\n", output);
        free(output);
    } else {
        printf("cat: %s: No such file\r\n", filename);
    }
}

bool filesystem_write_file(char *filename, char *text, int32_t length) {
    if (filesystem_get_free_space() <= 256) {
        printf("No free space!\n");
        return false;    
    }

    int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC);
    if (err < 0) return false;
    err = lfs_file_write(&eeprom_filesystem, &file, text, length);
    if (err < 0) return false;
    return lfs_file_close(&eeprom_filesystem, &file) == LFS_ERR_OK;
}

bool filesystem_append_file(char *filename, char *text, int32_t length) {
    if (filesystem_get_free_space() <= 256) {
        printf("No free space!\n");
        return false;    
    }

    int err = lfs_file_open(&eeprom_filesystem, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (err < 0) return false;
    err = lfs_file_write(&eeprom_filesystem, &file, text, length);
    if (err < 0) return false;
    return lfs_file_close(&eeprom_filesystem, &file) == LFS_ERR_OK;
}

int filesystem_cmd_ls(int argc, char *argv[]) {
    if (argc >= 2) {
        filesystem_ls(&eeprom_filesystem, argv[1]);
    } else {
        filesystem_ls(&eeprom_filesystem, "/");
    }
    return 0;
}

int filesystem_cmd_cat(int argc, char *argv[]) {
    (void) argc;
    filesystem_cat(argv[1]);
    return 0;
}

char* filesystem_get_b64encode_output(char *filename) {
    info.type = 0;
    lfs_stat(&eeprom_filesystem, filename, &info);
    if (!filesystem_file_exists(filename)) {
        return NULL;
    }

    if (info.size == 0) {
        char *empty = malloc(1);
        if (empty != NULL) {
            empty[0] = '\0';
        }
        return empty;
    }

    char *file_buf = malloc(info.size);
    if (file_buf == NULL) {
        return NULL;
    }

    if (!filesystem_read_file(filename, file_buf, info.size)) {
        free(file_buf);
        return NULL;
    }

    // Base64 encoding expands data by 4/3, plus null terminator and some padding
    lfs_size_t b64_size = ((info.size + 2) / 3) * 4 + 1;
    char *b64_buf = malloc(b64_size);
    if (b64_buf == NULL) {
        free(file_buf);
        return NULL;
    }

    b64_encode((unsigned char *)file_buf, info.size, (unsigned char *)b64_buf);
    free(file_buf);

    return b64_buf;
}

int filesystem_cmd_b64encode(int argc, char *argv[]) {
    (void) argc;
    char *b64_output = filesystem_get_b64encode_output(argv[1]);

    if (b64_output == NULL) {
        printf("b64encode: %s: No such file or error occurred\r\n", argv[1]);
        return -1;
    }

    if (b64_output[0] == '\0') {
        printf("\r\n");
        free(b64_output);
        return 0;
    }

    // Print in chunks with delays to avoid overwhelming serial output
    size_t len = strlen(b64_output);
    for (size_t i = 0; i < len; i += 16) {
        size_t chunk_len = min(16, len - i);
        printf("%.*s\n", (int)chunk_len, b64_output + i);
        delay_ms(10);
    }

    free(b64_output);
    return 0;
}

int filesystem_cmd_df(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    printf("free space: %ld bytes\r\n", filesystem_get_free_space());
    return 0;
}

int filesystem_cmd_rm(int argc, char *argv[]) {
    (void) argc;
    filesystem_rm(argv[1]);
    return 0;
}

int filesystem_cmd_format(int argc, char *argv[]) {
    (void) argc;
    if(strcmp(argv[1], "YES") == 0) {
        return _filesystem_format();
    }
    printf("usage: format YES\r\n");
    return 1;
}


int filesystem_cmd_echo(int argc, char *argv[]) {
    (void) argc;

    char *line = argv[1];
    size_t line_len = strlen(line);
    if (line[0] == '"' || line[0] == '\'') {
        line++;
        line_len -= 2;
        line[line_len] = '\0';
    }

    if (strchr(argv[3], '/')) {
        printf("subdirectories are not supported\r\n");
        return -2;
    }

    if (!strcmp(argv[2], ">")) {
        filesystem_write_file(argv[3], line, line_len);
        filesystem_append_file(argv[3], "\n", 1);
    } else if (!strcmp(argv[2], ">>")) {
        filesystem_append_file(argv[3], line, line_len);
        filesystem_append_file(argv[3], "\n", 1);
    } else {
        return -2;
    }

    return 0;
}
