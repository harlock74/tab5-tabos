#include <basic/extensions.h>
#include <basic/storage.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../upstream/glue.h"

enum {
    NAME_LIMIT    = 63,
    PATH_CAPACITY = 96,
    PROGRAM_START = 0x0801,
    PROGRAM_END   = 0xa000
};
static char filename[NAME_LIMIT + 1];
static bool name_valid;
static uint8_t device;
static uint8_t secondary;

static unsigned word(const unsigned char* bytes)
{
    return (unsigned) bytes[0] | (unsigned) bytes[1] << 8U;
}

void basic_storage_init(void)
{
    name_valid = false;
    device     = 1U;
    secondary  = 0U;
}

void basic_storage_name(void)
{
    const unsigned address = (unsigned) X | (unsigned) Y << 8U;
    const unsigned length  = A;
    name_valid             = false;
    if (length == 0U || length > NAME_LIMIT || address + length > sizeof(RAM)) {
        return;
    }
    if (memchr(RAM + address, 0, length) != NULL) {
        return;
    }
    memcpy(filename, RAM + address, length);
    filename[length] = '\0';
    name_valid       = true;
}

void basic_storage_device(void)
{
    device    = X;
    secondary = Y;
}

static const char* path_for(char* path, bool* default_directory)
{
    if (!name_valid) {
        return "INVALID FILENAME (1-63 BYTES REQUIRED)";
    }
    if ((device != 1U && device != 8U) || secondary != 0U) {
        return "UNSUPPORTED DEVICE OR SECONDARY ADDRESS";
    }
    const char* component = filename;
    *default_directory    = strchr(filename, ':') == NULL;
    if (!*default_directory) {
        if (!((filename[0] >= 'A' && filename[0] <= 'Z') || (filename[0] >= 'a' && filename[0] <= 'z')) ||
            filename[1] != ':' || filename[2] != '/') {
            return "INVALID TABOS PATH";
        }
        component += 3;
    } else if (strchr(filename, '/') != NULL) {
        return "USE A BARE NAME OR ABSOLUTE DRIVE PATH";
    }
    const char* start = component;
    for (const char* cursor = component;; ++cursor) {
        const unsigned char ch = (unsigned char) *cursor;
        if (ch == '/' || ch == 0U) {
            const size_t length = (size_t) (cursor - start);
            if (length == 0U || start[0] == '.' || start[0] == ' ' || cursor[-1] == '.' || cursor[-1] == ' ') {
                return "INVALID PATH COMPONENT";
            }
            if (ch == 0U) {
                break;
            }
            start = cursor + 1;
        } else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
                     ch == '-' || ch == '.' || ch == ' ')) {
            return "UNSUPPORTED FILENAME CHARACTER";
        }
    }
    (void) snprintf(path, PATH_CAPACITY, "%s%s", *default_directory ? "T:/basic/" : "", filename);
    return NULL;
}

// Require exact forward links and a final two-byte zero pointer. No relocation,
// executable machine-code tail, invalid tokens or out-of-arena addresses are accepted.
static bool valid_program(const unsigned char* bytes, size_t length)
{
    size_t offset     = 0U;
    unsigned previous = 0U;
    bool first        = true;
    while (offset + 2U <= length) {
        const unsigned next = word(bytes + offset);
        if (next == 0U) {
            return offset + 2U == length;
        }
        if (offset + 5U > length || next < PROGRAM_START || next >= PROGRAM_END) {
            return false;
        }
        const unsigned number = word(bytes + offset + 2U);
        if (number > 63999U || (!first && number <= previous)) {
            return false;
        }
        previous   = number;
        first      = false;
        size_t end = offset + 4U;
        while (end < length && bytes[end] != 0U) {
            if (end - offset - 4U >= 80U || bytes[end] < 32U || bytes[end] == 127U || bytes[end] > BASIC_TOKEN_LAST) {
                return false;
            }
            ++end;
        }
        if (end == length || next != PROGRAM_START + end + 1U) {
            return false;
        }
        offset = end + 1U;
    }
    return false;
}

static bool write_all(int fd, const unsigned char* bytes, size_t length)
{
    while (length != 0U) {
        const ssize_t count = write(fd, bytes, length);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0 || (size_t) count > length) {
            return false;
        }
        bytes  += count;
        length -= (size_t) count;
    }
    return true;
}

const char* basic_storage_save(void)
{
    char path[PATH_CAPACITY];
    bool default_directory;
    const char* error = path_for(path, &default_directory);
    if (error != NULL) {
        return error;
    }
    // SAVE passes a zero-page pointer in A and an exclusive end address in X/Y.
    const unsigned start = word(RAM + A);
    const unsigned end   = (unsigned) X | (unsigned) Y << 8U;
    if (A != 0x2bU || start != PROGRAM_START || end < start + 2U || end > PROGRAM_END ||
        !valid_program(RAM + start, end - start)) {
        return "INVALID PROGRAM SPAN";
    }
    if (default_directory && mkdir("T:/basic", 0777) != 0 && errno != EEXIST) {
        return "CANNOT CREATE T:/basic";
    }
    // O_EXCL avoids clobbering leftovers or another writer's temporary file.
    char temporary[PATH_CAPACITY];
    int fd = -1;
    for (unsigned attempt = 0U; attempt < 16U; ++attempt) {
        (void) snprintf(temporary, sizeof(temporary), "%s.tmp%u", path, attempt);
        fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd >= 0 || errno != EEXIST) {
            break;
        }
    }
    if (fd < 0) {
        return "CANNOT OPEN SAVE TEMPORARY FILE";
    }
    const unsigned char header[] = {1U, 8U};
    bool complete                = write_all(fd, header, sizeof(header)) && write_all(fd, RAM + start, end - start);
    if (close(fd) != 0) {
        complete = false;
    }
    if (!complete) {
        (void) unlink(temporary);
        return "SAVE WRITE/CLOSE FAILED";
    }
    if (rename(temporary, path) == 0) {
        return NULL;
    }
    if (errno != EEXIST) {
        return "SAVE RENAME FAILED (TEMPORARY FILE RETAINED)";
    }
    // ESP-IDF FAT rename does not replace an existing destination. Preserve it
    // under a separate name until promotion succeeds. Never remove it first.
    char backup[PATH_CAPACITY];
    (void) snprintf(backup, sizeof(backup), "%s.bak", path);
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return "SAVE DESTINATION IS NOT A REGULAR FILE";
    }
    if (stat(backup, &info) == 0 || errno != ENOENT) {
        return "SAVE BACKUP EXISTS OR IS INACCESSIBLE";
    }
    if (rename(path, backup) != 0) {
        return "SAVE BACKUP FAILED (TEMPORARY FILE RETAINED)";
    }
    if (rename(temporary, path) != 0) {
        if (rename(backup, path) != 0) {
            return "SAVE PROMOTION/RESTORE FAILED (BACKUP AND TEMP RETAINED)";
        }
        return "SAVE PROMOTION FAILED (OLD FILE RESTORED)";
    }
    if (unlink(backup) != 0) {
        return "SAVE COMPLETED BUT BACKUP CLEANUP FAILED";
    }
    return NULL;
}

const char* basic_storage_load(void)
{
    char path[PATH_CAPACITY];
    bool default_directory;
    const char* error = path_for(path, &default_directory);
    if (error != NULL) {
        return error;
    }
    if (A != 0U || word(RAM + 0x2bU) != PROGRAM_START || ((unsigned) X | (unsigned) Y << 8U) != PROGRAM_START) {
        return "UNSUPPORTED LOAD MODE";
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0 && errno == ENOENT && default_directory) {
        (void) snprintf(path, sizeof(path), "T:/data/basic/%s", filename);
        fd = open(path, O_RDONLY);
    }
    if (fd < 0) {
        return errno == ENOENT ? "FILE NOT FOUND" : "CANNOT OPEN LOAD FILE";
    }
    struct stat info;
    const size_t capacity = PROGRAM_END - PROGRAM_START + 2U;
    unsigned char* staged = NULL;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 4 || (uint64_t) info.st_size > capacity) {
        error = "INVALID PROGRAM FILE SIZE/TYPE";
    } else {
        staged = malloc((size_t) info.st_size);
        if (staged == NULL) {
            error = "OUT OF MEMORY STAGING LOAD";
        }
    }
    size_t used = 0U;
    while (error == NULL && used < (size_t) info.st_size) {
        const ssize_t count = read(fd, staged + used, (size_t) info.st_size - used);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0 || (size_t) count > (size_t) info.st_size - used) {
            error = "LOAD READ FAILED OR TRUNCATED";
        } else {
            used += (size_t) count;
        }
    }
    if (error == NULL) {
        unsigned char extra;
        ssize_t count;
        do {
            count = read(fd, &extra, 1U);
        } while (count < 0 && errno == EINTR);
        if (count != 0) {
            error = "LOAD FILE CHANGED OR READ FAILED";
        }
    }
    if (close(fd) != 0 && error == NULL) {
        error = "LOAD CLOSE FAILED";
    }
    if (error == NULL && (word(staged) != PROGRAM_START || !valid_program(staged + 2U, used - 2U))) {
        error = "INVALID TOKENIZED PROGRAM";
    }
    if (error == NULL) {
        // Only this commit mutates interpreter program memory. All fallible work
        // and file cleanup precede it. The core updates pointers on LOAD success.
        memcpy(RAM + PROGRAM_START, staged + 2U, used - 2U);
        const unsigned end = PROGRAM_START + (unsigned) used - 2U;
        X                  = (uint8_t) end;
        Y                  = (uint8_t) (end >> 8U);
    }
    free(staged);
    return error;
}
