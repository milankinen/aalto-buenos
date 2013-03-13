/*
 * touch.c
 */


#include "tests/lib.h"
#include "tests/str.h"

#define DISK "[disk1]"
#define BUFFER_SIZE 128
#define FILE_SIZE 512

int main(int argc,char **argv) {

    if (argc != 2) {
        cout("Invalid arguments. Usage: touch [filename]\n");
        return -1;
    }

    char *filename = argv[1];
    char filepath[BUFFER_SIZE];
    snprintf(filepath, BUFFER_SIZE, "%s%s", DISK, filename);

    int retval = syscall_create(filepath, FILE_SIZE);
    if (retval < 0) {
        cout("Error creating file %s \n", filename);
        return retval;
    }

    return 1;
}
