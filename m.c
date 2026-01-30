#include <stdio.h>
#include <string.h>

/* A simplistic non-reentrant path builder that uses a shared buffer */
static char shared_buf[256];

const char *fake_path_join(const char *base, const char *name) {
    /* override shared_buf */
    shared_buf[0] = '\0';
    strcat(shared_buf, base);
    strcat(shared_buf, name);
    return shared_buf;
}

int main(void) {
    const char *p1;
    const char *p2;

    p1 = fake_path_join("/foo/", "frob1");
    p2 = fake_path_join("/bar/", "barf2");

    /* The first return pointer refers into shared_buf too */
    printf("p1: %s\n", p1);
    printf("p2: %s\n", p2);

    return 0;
}