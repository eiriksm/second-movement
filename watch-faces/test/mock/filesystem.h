#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stdint.h>

bool filesystem_write_file(char *filename, char *text, int32_t length);
bool filesystem_append_file(char *filename, char *text, int32_t length);

// In-memory store behind the functions above.
void filesystem_mock_reset(void);
// Returns the file's contents as a string, or NULL if it does not exist.
const char *filesystem_mock_get_file(const char *filename);

#endif // FILESYSTEM_H
