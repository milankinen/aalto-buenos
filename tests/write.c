/*
 * write.c
 */

/*create a file, dont close the filehandle
 * and return the unclosed filehandle
 */

#include "tests/lib.h"

int main(int argc, char** argv){
    argc = argc;
    char *filename = argv[1];
    syscall_create(filename, 512);
    return syscall_open(filename);
}
