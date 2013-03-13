/*
 * append.c
 *
 * append text to file

 * touch.c
 */


#include "tests/lib.h"
#include "tests/str.h"

#define DISK "[disk1]"
#define BUFFER_SIZE 128


int main(int argc,char **argv) {

    if (argc != 3) {
        cout("Invalid arguments. Usage: append [filename] [text]\n");
        return -1;
    }

    char *filename = argv[1];
    char filepath[BUFFER_SIZE ];
    snprintf(filepath, BUFFER_SIZE , "%s%s", DISK, filename);

    char *text = argv[2];
    int filehandle = syscall_open(filepath);
    if(filehandle < 0){
        cout("Could not open file: %s", filename);
    }
    int retval = syscall_write(filehandle, text, strlen(text));
    syscall_close(filehandle);
    if (retval < 0) {
        cout("Error writing to file %s \n", filename);
        return retval;
    }

    return 1;
}

