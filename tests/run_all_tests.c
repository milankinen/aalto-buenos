#include "tests/lib.h"
#include "tests/str.h"

static int test_num;
static char tests[10][8][20];
static int cases[10];

static int create_test_suite(const char* executable) {
    int num = test_num;
    stringcopy(tests[num][0], executable, 20);
    cases[num] = 0;
    test_num++;
    return num;
}

static void add_test_case(int suite, const char* arg) {
    cases[suite]++;
    stringcopy(tests[suite][cases[suite]], arg, 20);
}

static void run_suites() {
    int i, j, retval;
    char filename[20];
    int test_pid;
    char* argv[1];

    for (i = 0 ; i < test_num ; i++) {
       snprintf(filename, 20, "[disk1]%s", tests[i][0]);
       for (j = 0 ; j < cases[i] ; j++) {
           argv[0] = tests[i][j+1];
           cout("Running test %s [%s]", filename, argv[0]);
           test_pid = syscall_execp((const char*)filename, 1, (const char**)argv);
           if (test_pid >= 0) {
               retval = syscall_join(test_pid);
               cout("Test (PID: %d) over, return value: %d", test_pid, retval);
           }
       }
   }
}


int main() {

    int testsuite;
    test_num = 0;

    testsuite = create_test_suite("test_join");
    add_test_case(testsuite, "basic");


    run_suites();
    // all tests executed
    syscall_halt();
    return 0;
}
