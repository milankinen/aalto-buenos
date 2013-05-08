#include "tests/lib.h"
#include "tests/str.h"

int
main(void) {
    cout("Running malloc tests: ");
    char *s = malloc(sizeof(char));
    free(s);
    cout("OK.\n");
    return 0;
}
