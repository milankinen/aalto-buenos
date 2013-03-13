/*
 * cat.c
 */

#include "tests/lib.h"
#include "tests/str.h"

#define DISK "[disk1]"


int main(int argc,char **argv) {

    if (argc != 2) {
        cout("Invalid arguments. Usage: cat [filename]\n");
        return -1;
    }
    int BUFFER_SIZE = 128;

    char* buffer[BUFFER_SIZE];
    char *filename = argv[1];
    char filepath[128];
    snprintf(filepath, 128, "%s%s", DISK, filename);
    int filehandle = syscall_open(filepath);
    if (filehandle < 0) {
        cout("File %s not found\n", filename);
        syscall_close(filehandle);
        return filehandle;
    }
    int retval;
    while (1) {
        retval = syscall_read(filehandle, buffer, BUFFER_SIZE);
        if (retval < 0) {
            cout("Error when reading file\n");
            syscall_close(filehandle);
            return retval;
        }
        syscall_write(1, buffer, BUFFER_SIZE);

        if(retval < BUFFER_SIZE){
            break;
        }
    }
    syscall_close(filehandle);
    return 1;
}
