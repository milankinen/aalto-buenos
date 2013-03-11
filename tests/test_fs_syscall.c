
#include "tests/lib.h"
#include "tests/str.h"
#include "fs/vfs.h"
#include "tests/str.h"

void message(char *message) {
    syscall_write(1, message, strlen(message));
}

void assert(int expected, int value, char *message) {
    char temp[50];

    if (expected != value) {
        snprintf(temp, 50, "expected: %d, got: %d\n", expected, value);
        syscall_write(1, message, strlen(message));
        syscall_write(1, temp, strlen(temp));
    }
}

void test_open_close() {

    /*open nonexisting file*/
    int filehandle = syscall_open("[disk1]no_such_file.txt");
    assert(filehandle, VFS_ERROR, "test_open_close_1_failed\n");

    /*open existing file*/
    int filehandle2 = syscall_open("[disk1]test.txt");
    assert(filehandle2, 3, "test_open_close_2_failed\n");

    /*close nonexisting file*/
    int return_value1 = syscall_close(6);
    assert(return_value1, VFS_NOT_OPEN, "test_open_close_3_failed\n");

    /*close existing file*/
    int return_value2 = syscall_close(filehandle2);
    assert(return_value2, 0, "test_open_close_4_failed\n");

}

void test_read() {
    int bufsize = 50;
    char buffer[bufsize];

    const char* TEXT = "first row of text";
    const char* STUB_TEXT = "first";
    int return_value;

    /*read the whole file and check that content match*/
    int filehandle = syscall_open("[disk1]test.txt");
    assert(filehandle, 3, "test_read_1_failed\n");

    return_value = syscall_read(filehandle, buffer, bufsize);
    assert(strlen(TEXT), return_value, "test_read_2_failed\n");

    assert(0, stringcmp(TEXT, buffer), "test_read_3_failed\n");

    return_value = syscall_close(filehandle);
    assert(return_value, 0, "test_read_4_failed\n");

    /*read the same file again, but only 5 bytes */
    int i;
    for (i = 0; i < bufsize; i++) {
        buffer[i] = '\0';
    }
    filehandle = syscall_open("[disk1]test.txt");
    return_value = syscall_read(filehandle, buffer, 5);
    assert(return_value, 5, "test_read_5_failed\n");
    assert(strlen(buffer), 5, "test_read_6_failed\n");
    assert(0, stringcmp(buffer, STUB_TEXT), "test_read_7_failed\n");
}

void test_write() {
    int bufsize = 50;
    char buffer[bufsize];
    char *TEXT = "WRITTEN TEXT";
    /*write text to file */
    int filehandle = syscall_open("[disk1]test.txt");
    int return_value = syscall_write(filehandle, TEXT, strlen(TEXT));
    assert(strlen(TEXT), return_value, "test_write_1_failed\n");

    /*read text from file*/
    syscall_seek(filehandle, 0);
    syscall_read(filehandle, buffer, strlen(TEXT));

    /*check written and read text matches*/
    assert(stringcmp(TEXT, buffer), 0, "test_write_2_failed\n");
    syscall_close(filehandle);
}

/*NOT in use*/
void test_read_stdin() {
    char buf[50];
    int i;
    for (i = 0; i < 50; i++) {
        buf[i] = '\0';
    }
    syscall_read(0, buf, 50);
    syscall_write(1, "u typed\n", strlen("u typed\n"));
    syscall_write(1, buf, 50);
}

int main(void) {

    test_open_close();
    test_read();
    test_write();

    syscall_halt();
    return 0;
}
