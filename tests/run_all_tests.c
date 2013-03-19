#include "tests/lib.h"
#include "tests/str.h"

static int test_num;
static char tests[10][8][20];
static int cases[10];

static int create_test_suite(const char* executable);
static void add_test_case(int suite, const char* arg);
static void run_suites();


int main() {

    int testsuite;
    test_num = 0;

    testsuite = create_test_suite("test_proc");
    add_test_case(testsuite, "basic");
    add_test_case(testsuite, "nochild");
    add_test_case(testsuite, "maxprocs");
    add_test_case(testsuite, "nofile");
    add_test_case(testsuite, "invldexe");
    add_test_case(testsuite, "maxargs");

    testsuite = create_test_suite("test_panic");
    add_test_case(testsuite, "stack");
    add_test_case(testsuite, "krnlLdaddr");
    add_test_case(testsuite, "krnlSaddr");
    add_test_case(testsuite, "nullLdaddr");
    add_test_case(testsuite, "nullSaddr");
    add_test_case(testsuite, "rndLdaddr");
    add_test_case(testsuite, "rndSaddr");
    // second suite, 7 is max per suite
    testsuite = create_test_suite("test_panic");
    add_test_case(testsuite, "frmViolat");
    add_test_case(testsuite, "jmpKernel");
    // process related syscalls
    add_test_case(testsuite, "negArgc");
    add_test_case(testsuite, "manyArgc");
    add_test_case(testsuite, "nullArgv");
    add_test_case(testsuite, "noTermArgv");
    add_test_case(testsuite, "krnlArgv");

    testsuite = create_test_suite("test_fs_syscall");
    add_test_case(testsuite, "test_open_close");
    add_test_case(testsuite, "test_read");
    add_test_case(testsuite, "test_write");
    add_test_case(testsuite, "test_create");
    add_test_case(testsuite, "test_delete");
    add_test_case(testsuite, "test_complex");

    run_suites();
    // all tests executed
    cout("Wooot? All tests ok? Check results.\n");
    syscall_halt();
    return 0;
}



static int create_test_suite(const char* executable) {
    int num = test_num;
    stringcopy(tests[num][0], executable, 30);
    cases[num] = 0;
    test_num++;
    return num;
}

static void add_test_case(int suite, const char* arg) {
    cases[suite]++;
    stringcopy(tests[suite][cases[suite]], arg, 30);
}

static void run_suites() {
    int i, j, retval;
    char filename[30];
    int test_pid;
    char* argv[1];

    for (i = 0 ; i < test_num ; i++) {
       snprintf(filename, 30, "[disk1]%s", tests[i][0]);
       for (j = 0 ; j < cases[i] ; j++) {
           argv[0] = tests[i][j+1];
           cout("Running test %s [%s]\n", filename, argv[0]);
           test_pid = syscall_execp((const char*)filename, 1, (const char**)argv);
           if (test_pid >= 0) {
               retval = syscall_join(test_pid);
               cout("Test (PID: %d) over, return value: %d\n", test_pid, retval);
           } else {
               cout("Test starting failed: %d\n", test_pid);
           }
       }
   }
}


