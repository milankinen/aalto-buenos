
#include "tests/lib.h"
#include "tests/str.h"


static int run_basic() {
    int pid, retval;
    cout("** Test basic join\n");
    pid = syscall_exec("[disk1]test_proc");
    retval = syscall_join(pid);
    cout("** Basic join complete, return value %d (expected 9).\n", retval);
    cout("** Now exiting with 45...\n");
    syscall_exit(45);
    cout("** This should be never reached!\n");
    return 0;
}

static int run_nochild() {
    int retval;
    cout("** Test no child join\n");
    retval = syscall_join(-1);
    cout("** No child join complete, should return negative, returned: %d\n", retval);
    return 0;
}

static int run_max_processes() {
    int pid, num;
    cout("** Test max processes\n");
    num = 0;
    while (++num) {
        cout("*** Starting child processs #%d\n", num);
        pid = syscall_exec("[disk1]test_proc");
        if (pid < 0) {
            cout("** Reached max limit, exiting..\n");
            return num;
        }
    }
    return 0;
}

static int run_no_file() {
    int pid;
    cout("** Trying to start process with non-existing filename\n");
    pid = syscall_exec("[dsi]no_exists");
    cout("** Got %d\n", pid);
    return 0;
}

static int invalid_executable() {
    int pid;
    cout("** Trying to start invalid executable file\n");
    pid = syscall_exec("[disk1]bigfile.txt");
    cout("** Got %d\n", pid);
    return 0;
}

#define ARGS_TEST 31

static int run_maximum_arguments() {
    char* argv[ARGS_TEST];
    int i;
    for (i = 0 ; i < ARGS_TEST ; i++) {
        argv[i] = "a";
    }
    cout("** Trying to start with %d arguments\n", ARGS_TEST);
    i = syscall_execp("[disk1]test_proc", ARGS_TEST, (const char**)argv);
    cout("** Got %d\n", i);
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
    } else if (stringcmp(argv[1], "maxprocs") == 0) {
        return run_max_processes();
    } else if (stringcmp(argv[1], "nofile") == 0) {
        return run_no_file();
    } else if (stringcmp(argv[1], "invldexe") == 0) {
        return invalid_executable();
    } else if (stringcmp(argv[1], "maxargs") == 0) {
        return run_maximum_arguments();
    }
    return 0;
}
