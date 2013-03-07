
#include "tests/lib.h"
#include "tests/str.h"


int main(int argc, char** argv) {

    int pid;

    char a1[16];
    char a2[16];
    char a3[16];
    char* args[3];

    argv = argv;
    stringcopy(a1, "foobar", 16);
    stringcopy(a2, "barfoox", 16);
    stringcopy(a3, "barbarxx", 16);

    args[0] = a1;
    args[1] = a2;
    args[2] = a3;

    argc++;
    if (argc <= 3) {
        pid = syscall_execp("[disk1]test_join", argc, (const char**)args);
        if (syscall_join(pid) < 0) {
            return 666;
        }
    }

    if (--argc == 1) {
        syscall_halt();
    }
    return argc;
}
