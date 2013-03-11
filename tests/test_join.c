
#include "tests/lib.h"
#include "tests/str.h"


static int run_basic(int argc, char** argv) {
    char a1[16];
    char a2[16];
    char a3[16];
    char* args[4];
    int pid;

    argv = argv;
    stringcopy(a1, "basic", 16);
    stringcopy(a2, "asdasd", 16);
    stringcopy(a3, "asdasdasd", 16);
    args[0] = a1;
    args[1] = a2;
    args[2] = a3;

    // we must pass "basic" attribute so that child process identifies the
    // right test case
    if (argc == 0) argc = 1;
    argc++;
    if (argc <= 3) {
        pid = syscall_execp("[disk1]test_join", argc, (const char**)args);
        if (syscall_join(pid) < 0) {
            return 666;
        }
    }
    return argc;
}


int main(int argc, char** argv) {
    if (stringcmp(argv[1], "basic")) {
        return run_basic(argc - 1, argv + 1);
    }
    return 0;
}
