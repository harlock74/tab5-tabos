#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

unsigned char RAM[65536], A, X, Y, S, V, B, D, I, C, N, Z;
unsigned short PC;
static bool fail_open, fail_directory, fail_write, fail_close, short_read, fat_rename, fail_promote;
static size_t written, consumed;

static int storage_open(const char* path, int flags, ...)
{
    if (fail_open) {
        errno = EROFS;
        return -1;
    }
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        const int mode = va_arg(args, int);
        va_end(args);
        return open(path, flags, mode);
    }
    return open(path, flags);
}

static int storage_mkdir(const char* path, mode_t mode)
{
    if (fail_directory) {
        errno = ENODEV;
        return -1;
    }
    return mkdir(path, mode);
}

static ssize_t storage_write(int fd, const void* data, size_t size)
{
    if (fail_write && written >= 2U) {
        errno = ENOSPC;
        return -1;
    }
    if (size > 3U) {
        size = 3U; // Every successful SAVE must handle positive partial writes.
    }
    const ssize_t count = write(fd, data, size);
    if (count > 0) {
        written += (size_t) count;
    }
    return count;
}

static ssize_t storage_read(int fd, void* data, size_t size)
{
    if (short_read && consumed >= 3U) {
        return 0;
    }
    if (size > 3U) {
        size = 3U;
    }
    const ssize_t count = read(fd, data, size);
    if (count > 0) {
        consumed += (size_t) count;
    }
    return count;
}

static int storage_close(int fd)
{
    const int result = close(fd);
    if (fail_close) {
        errno = EIO;
        return -1;
    }
    return result;
}

static int storage_rename(const char* src, const char* dst)
{
    struct stat info;
    if (fat_rename && stat(dst, &info) == 0) {
        errno = EEXIST;
        return -1;
    }
    if (fail_promote && strstr(src, ".tmp") != NULL) {
        errno = EIO;
        return -1;
    }
    return rename(src, dst);
}

// Compile the unchanged production storage adapter against fault-injected POSIX
// calls. Other native/RV32 tests link it normally through their own SDK boundary.
#define open   storage_open
#define mkdir  storage_mkdir
#define write  storage_write
#define read   storage_read
#define close  storage_close
#define rename storage_rename
#include "../src/basic_storage.c"
#undef open
#undef mkdir
#undef write
#undef read
#undef close
#undef rename

static void name(const char* value)
{
    memcpy(RAM + 0xf000U, value, strlen(value));
    A = (uint8_t) strlen(value);
    X = 0U;
    Y = 0xf0U;
    basic_storage_name();
}

static void program(unsigned char token)
{
    const unsigned char bytes[] = {7, 8, 10, 0, token, 0, 0, 0};
    memcpy(RAM + PROGRAM_START, bytes, sizeof(bytes));
    RAM[0x2b] = 1U;
    RAM[0x2c] = 8U;
}

static const char* save(void)
{
    A       = 0x2bU;
    X       = 9U;
    Y       = 8U;
    written = 0U;
    return basic_storage_save();
}

static const char* load(void)
{
    A        = 0U;
    X        = 1U;
    Y        = 8U;
    consumed = 0U;
    return basic_storage_load();
}

static void unchanged(const unsigned char* snapshot)
{
    assert(memcmp(snapshot, RAM, sizeof(RAM)) == 0);
}

int main(void)
{
    char root[] = "/tmp/tabos-basic-storage-XXXXXX";
    assert(mkdtemp(root) != NULL && chdir(root) == 0 && mkdir("T:", 0700) == 0);
    basic_storage_init();
    name("HELLO");
    program(0x80U);
    fail_directory = true;
    assert(save() != NULL);
    fail_directory = false;
    assert(save() == NULL);
    assert(written == 10U);
    // Confirm exact PRG bytes after positive partial writes.
    unsigned char expected[] = {1, 8, 7, 8, 10, 0, 0x80, 0, 0, 0};
    unsigned char actual[sizeof(expected)];
    int fd = open("T:/basic/HELLO", O_RDONLY);
    assert(fd >= 0 && read(fd, actual, sizeof(actual)) == sizeof(actual) && close(fd) == 0);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(mkdir("T:/data", 0700) == 0 && mkdir("T:/data/basic", 0700) == 0);
    fd = open("T:/data/basic/PACKAGED.prg", O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0 && write_all(fd, expected, sizeof(expected)) && close(fd) == 0);
    name("PACKAGED.prg");
    program(0x8fU);
    assert(load() == NULL && RAM[PROGRAM_START + 4U] == 0x80U);
    program(0x8fU);
    assert(save() == NULL);
    program(0x80U);
    assert(load() == NULL && RAM[PROGRAM_START + 4U] == 0x8fU);
    assert(unlink("T:/basic/PACKAGED.prg") == 0);
    name("HELLO");
    program(0x8fU);
    fail_write = true;
    assert(save() != NULL);
    fail_write = false;
    fail_close = true;
    assert(save() != NULL);
    fail_close = false;
    assert(load() == NULL && RAM[PROGRAM_START + 4U] == 0x80U);
    // FAT overwrite fallback, followed by rollback after failed promotion.
    fat_rename = true;
    program(0x8fU);
    assert(save() == NULL);
    program(0x80U);
    fail_promote = true;
    assert(save() != NULL);
    fail_promote = false;
    assert(load() == NULL && RAM[PROGRAM_START + 4U] == 0x8fU);
    assert(unlink("T:/basic/HELLO.tmp0") == 0);
    unsigned char snapshot[sizeof(RAM)];
    memcpy(snapshot, RAM, sizeof(RAM));
    short_read = true;
    assert(load() != NULL);
    unchanged(snapshot);
    short_read = false;
    fail_close = true;
    assert(load() != NULL);
    unchanged(snapshot);
    fail_close = false;
    fail_open  = true;
    assert(load() != NULL && save() != NULL);
    unchanged(snapshot);
    fail_open = false;
    // SETNAM never changes RAM, including nonterminated end-of-RAM sources.
    A = 2U;
    X = 255U;
    Y = 255U;
    basic_storage_name();
    assert(load() != NULL);
    unchanged(snapshot);
    RAM[0xf000U] = 'A';
    RAM[0xf001U] = 0U;
    A            = 2U;
    X            = 0U;
    Y            = 0xf0U;
    basic_storage_name();
    assert(load() != NULL);
    name("HELLO");
    // Structurally malformed mutations are rejected without committing live RAM.
    const unsigned char corrupt[][10] = {
        {0, 0xc0, 7, 8,  10,   0, 0x80, 0, 0, 0}, // wrong load address
        {1,    8, 1, 8,  10,   0, 0x80, 0, 0, 0}, // backwards link
        {1,    8, 7, 8, 255, 255, 0x80, 0, 0, 0}, // invalid line number
        {1,    8, 7, 8,  10,   0, 0xff, 0, 0, 0}, // invalid token
        {1,    8, 7, 8,  10,   0, 0x80, 1, 0, 0}, // missing line terminator
    };
    memcpy(snapshot, RAM, sizeof(RAM));
    for (size_t i = 0U; i < sizeof(corrupt) / sizeof(corrupt[0]); ++i) {
        fd = open("T:/basic/HELLO", O_WRONLY | O_TRUNC);
        assert(fd >= 0 && write(fd, corrupt[i], sizeof(corrupt[i])) == sizeof(corrupt[i]) && close(fd) == 0);
        assert(load() != NULL);
        unchanged(snapshot);
    }
    assert(unlink("T:/basic/HELLO") == 0 && rmdir("T:/basic") == 0);
    assert(unlink("T:/data/basic/PACKAGED.prg") == 0 && rmdir("T:/data/basic") == 0 && rmdir("T:/data") == 0);
    assert(rmdir("T:") == 0);
    assert(chdir("/") == 0 && rmdir(root) == 0);
    puts("BASIC storage: partial I/O, faults, FAT rollback and RAM preservation passed");
    return 0;
}
