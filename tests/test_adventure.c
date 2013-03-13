
#include "kernel/config.h"
#include "tests/lib.h"
#include "tests/str.h"
#include "proc/syscall.h"



#define NUM_SYSCALL 9

#define NUM_STRS 6
#define NUM_INTS 4
#define NUM_DYN 1 // = vargv
#define NUM_PARAMS (NUM_STRS+NUM_INTS+NUM_DYN)


static int syscalls[NUM_SYSCALL] = {
 //SYSCALL_HALT,
 //SYSCALL_EXEC,
 //SYSCALL_EXIT,
 //SYSCALL_JOIN,
 //SYSCALL_FORK,
 //SYSCALL_MEMLIMIT,
 SYSCALL_OPEN,
 SYSCALL_CLOSE,
 SYSCALL_SEEK,
 SYSCALL_READ,
 SYSCALL_WRITE,
 SYSCALL_CREATE,
 SYSCALL_DELETE,
 0x307,
 0
};

static const char* strs[NUM_STRS] = {
 NULL,
 NULL,  // this will be added in main (large string)
 NULL,  // this will be added in main (no terminator)
 "foobar",
 "[disk1]test_join",
 (const char*)(0x80000000 | 0xdeadbeef),
};

static int ints[NUM_INTS] = {
 1,
 -1,
 0xffffffff,
 1337
};


int main(int argc, char** argv) {
    int i, j;
    uint32_t params[NUM_PARAMS];
    char large[520];
    char no_terminal[10];
    char* vargv[1];
    int a1, a2, a3;
    uint32_t syscallnum, retval;

    char* pargv[4];
    char  pa1[15];
    char  pa2[15];
    char  pa3[15];
    char  psys[20];
    int pid, pretval;


    memoryset(large, 'a', 520);
    large[520 - 1] = '\0';
    memoryset(no_terminal, 'a', 10);

    vargv[0] = (char*)strs[5];

    strs[1] = large;
    strs[2] = no_terminal;
    //cout("asdsad\n");

    for (i = 0 ; i < NUM_STRS ; i++) {
        params[i] = (uint32_t)strs[i];
    }
    for (j = 0 ; j < NUM_INTS; j++, i++) {
        params[i] = (uint32_t)ints[j];
    }
    params[i++] = (uint32_t)vargv;

    //cout("foobar\n");

    if (argc > 4) {
        syscallnum = atoi(argv[1]);
        a1 = atoi(argv[2]);
        a2 = atoi(argv[3]);
        a3 = atoi(argv[4]);
        cout("Testing syscall %d with argument indexes a1=%d a2=%d a3=%d\n", syscallnum, a1, a2, a3);
        retval = _syscall(syscallnum, params[a1], params[a2], params[a3]);
        return (int)retval;
    } else {
        for (syscallnum = 0 ; syscallnum < NUM_SYSCALL ; syscallnum++) {
            for (a1 = 0 ; a1 < NUM_PARAMS ; a1++) {
                for (a2 = 0 ; a2 < NUM_PARAMS ; a2++) {
                    for (a3 = 0 ; a3 < NUM_PARAMS ; a3++) {
                        snprintf(pa1, 15, "%d", a1);
                        snprintf(pa2, 15, "%d", a2);
                        snprintf(pa3, 15, "%d", a3);
                        snprintf(psys, 20, "%d", syscalls[syscallnum]);
                        pargv[0] = psys;
                        pargv[1] = pa1;
                        pargv[2] = pa2;
                        pargv[3] = pa3;
                        cout("Starting test syscall %d with argument indexes a1=%d a2=%d a3=%d\n", syscalls[syscallnum],
                                a1, a2, a3);
                        pid = syscall_execp("[disk1]test_adventure", 4, (const char**)pargv);
                        if (pid < 0) {
                            cout("Test starting failed\n");
                        }
                        pretval = syscall_join(pid);
                        cout("Test ok, retval %d\n", pretval);
                    }
                }
            }
        }
    }

    cout("All tests run happily and kernel is still up! ;)\n");
    if (!(argc > 1 && stringcmp("nohalt", argv[1]))) {
        syscall_halt();
    }
    return 0;
}
