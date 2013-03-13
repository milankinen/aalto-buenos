
#include "tests/lib.h"
#include "tests/str.h"


static int _stack_sz_recursive() {
    char buf[512];
    buf[0] = 1;
    return (int)buf[0] + _stack_sz_recursive();
}

static int run_stack_size_limit() {
    int retval = 0;
    retval += _stack_sz_recursive();
    return retval;
}

static int run_load_addr(int* ptr) {
    return *ptr;
}

static int run_store_addr(int* ptr) {
    return *ptr = 1337;
}


static int run_jump_to_kernel(int foo) {
    int i;
    int* addr;
    foo = foo;
    // try to override return address in stack frame so that return jump transfers
    // us to kernel space ;)
    for (i = -2 ; i < 4 ; i++) {
        addr = ((&foo) + i);
        //cout("Write %d to addr %d\n", 0x80000000 | 0xdeadbeef, addr);
        *addr = 0x80000000 | 0xdeadbeef;
    }
    return 0;
}



/* PROC SYSCALLS */
static int run_negative_argc() {
    const char* a = "asd";
    char* argv[1];
    argv[0] = (char*)a;
    return syscall_execp("[disk]test_panic", -1, (const char**)argv);
}

static int run_too_many_argc() {
    const char* a = "asd";
    char* argv[1];
    argv[0] = (char*)a;
    return syscall_execp("[disk]test_panic", 5, (const char**)argv);
}

static int run_null_in_argv() {
    return syscall_execp("[disk]test_panic", 2, NULL);
}

static int run_no_terminator_in_argv() {
    char arg[10];
    char* argv[1];
    // construct array without ending '\0' to cause buffer overflow
    memoryset(arg, 'a', 10);
    argv[0] = arg;
    return syscall_execp("[disk]test_panic", 5, (const char**)argv);
}

static int run_argv_with_kernel_ptr() {
    return syscall_execp("[disk]test_panic", 1, (const char**)(0x80000000 | 0xdeadbeef));
}




int main(int argc, char** argv) {
    if (argc <= 1) {
        return 9;
    }

    /* memory store/load from userland to cause panic */
    if (stringcmp(argv[1], "stack") == 0) {
        return run_stack_size_limit();
    } else if (stringcmp(argv[1], "krnlLdaddr") == 0) {
        return run_load_addr((int*)(0x80000000 | 0xdeadbeef));
    } else if (stringcmp(argv[1], "krnlSaddr") == 0) {
        return run_store_addr((int*)(0x80000000 | 0xdeadbeef));
    } else if (stringcmp(argv[1], "nullLdaddr") == 0) {
        return run_load_addr(NULL);
    } else if (stringcmp(argv[1], "nullSaddr") == 0) {
        return run_store_addr(NULL);
    } else if (stringcmp(argv[1], "rndLdaddr") == 0) {
        while(argv++) { run_load_addr((int*)argv); }
    } else if (stringcmp(argv[1], "rndSaddr") == 0) {
        while(argv++) { run_store_addr((int*)argv); }
    } else if (stringcmp(argv[1], "frmViolat") == 0) {
        // write just about override main return address in stack frame and try cause panic
        *((&argc) - 1) = 0;
    } else if (stringcmp(argv[1], "jmpKernel") == 0) {
        return run_jump_to_kernel(0);
    }

    /* memory store/load with systemcalls to cause panic */
    else if (stringcmp(argv[1], "negArgc") == 0) {
        return run_negative_argc();
    } else if (stringcmp(argv[1], "manyArgc") == 0) {
        return run_too_many_argc();
    } else if (stringcmp(argv[1], "nullArgv") == 0) {
        return run_null_in_argv();
    } else if (stringcmp(argv[1], "noTermArgv") == 0) {
        return run_no_terminator_in_argv();
    } else if (stringcmp(argv[1], "krnlArgv") == 0) {
        return run_argv_with_kernel_ptr();
    }

    return 0;
}

