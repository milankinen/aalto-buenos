#include "tests/lib.h"
#include "tests/str.h"

#define DISK "[disk1]"
#define BUFFER_SIZE 128
#define DIR_SIZE 512


static int is_folder(char* path) {
    return strlen(path) > 0 && path[strlen(path) - 1] == '/';
}

int main(int argc, char **argv) {

    if (argc != 2) {
        cout("Invalid arguments. Usage: ls [path]\n");
        return -1;
    }

    char *path = argv[1];
    char filepath[BUFFER_SIZE];
    char dir[DIR_SIZE];

    snprintf(filepath, BUFFER_SIZE, "%s%s%s", DISK, path, is_folder(path) ? "" : "/");
    int handle = syscall_open(filepath);
    if (handle < 0) {
        cout("Folder not found %s%s\n", path, is_folder(path) ? "" : "/");
        syscall_exit(-1);
    }

    if (syscall_read(handle, dir, DIR_SIZE) < 0) {
        cout("ls unknown error, filesystem corrupted\n");
        syscall_exit(-2);
    }

    syscall_close(handle);
    uint32_t num = *((uint32_t*)dir);
    uint32_t i;
    char* p = dir + sizeof(uint32_t);
    cout("\n");
    for (i = 0 ; i < num ; i++) {
        stringcopy(filepath, p, BUFFER_SIZE);
        p += strlen(filepath) + 1;
        cout("%s ", filepath);
    }
    cout("\n");
    return 0;
}
