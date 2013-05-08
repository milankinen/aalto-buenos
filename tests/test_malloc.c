#include "tests/lib.h"
#include "tests/str.h"

static void
ok(void *b) {
    if (b) cout("OK.\n");
    else cout("FAIL!\n");
}

int
main(void) {
    char *s,*t,*u;

    cout("Running malloc tests: \n");

    cout("-> Small allocation: ");
    s = malloc(sizeof(char));
    ok(s);

    cout("-> Small free: ");
    free(s);
    cout("Done.");

    cout("-> Large allocation: ");
    s = malloc(4096*sizeof(char));
    ok(s);

    cout("-> Large allocation: ");
    t = malloc(4096*sizeof(char));
    ok(t);

    cout("-> Large free: ");
    free(s);
    cout("Done.");

    cout("-> Small allocation: ");
    s = malloc(8*sizeof(char));
    ok(s);

    cout("-> Small allocation: ");
    u = malloc(8*sizeof(char));
    ok(u);

    cout("-> Small free: ");
    free(s);
    cout("Done.");

    cout("-> Large free: ");
    free(t);
    cout("Done.");

    cout("-> Large allocation: ");
    t = malloc(1024*sizeof(char));
    ok(t);

    cout("-> Small free: ");
    free(u);
    cout("Done.");

    cout("-> Large free: ");
    free(t);
    cout("Done.");

    cout("-> Free NULL: ");
    free(NULL);
    cout("Done.");

    cout("-> Malloc 0: ");
    s = malloc(0);
    if (s) cout("Fail!\n");
    else cout("OK.\n");

    return 0;
}
