#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
die(const char *msgf, ...) {
    va_list ap;
    va_start(ap, msgf);
    vfprintf(stderr, msgf, ap);
    va_end(ap);

    if (msgf[strlen(msgf)-1] == ':') {
        perror(NULL);
    }

    exit(1);
}

void *
xcalloc(size_t nmemb, size_t sz) {
    void *p = calloc(nmemb, sz);
    if (!p)
        die("calloc():");
    return p;
}

void *
clone(void *p, size_t n) {
    void *p_ = malloc(n);
    memcpy(p_, p, n);
    return p_;
}
