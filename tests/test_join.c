
#include "tests/lib.h"
#include "tests/str.h"


static int run_basic() {
    int pid, retval;
    cout("** Test basic join\n");
    pid = syscall_exec("[disk1]test_join");
    retval = syscall_join(pid);
    cout("** Basic join complete, return value %d (expected 9).\n", retval);
    return 0;
}

static int run_nochild() {
    int retval;
    cout("** Test no child join\n");
    //pid = syscall_exec("[disk1]test_join");
    retval = syscall_join(-1);
    cout("** No child join complete, should return negative, returned: %d\n", retval);
    return 0;
}


int main(int argc, char** argv) {
    if (argc <= 1) {
        return 9;
    }
    if (stringcmp(argv[1], "basic") == 0) {
        return run_basic();
    } else if (stringcmp(argv[1], "nochild") == 0) {
        return run_nochild();
    }
    return 0;
}
