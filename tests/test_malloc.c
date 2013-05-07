#include "tests/lib.h"
#include "tests/str.h"

int
main(void) {
    cout("Running malloc tests.\n");
    char *s = malloc(sizeof(char));
    free(s);
    syscall_halt();
    return 0;
}
