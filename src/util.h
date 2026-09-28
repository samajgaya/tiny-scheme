#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMPZ(a) ((a) > 0 ? (a) : 0)

#define LEN(xs) (sizeof(xs)/sizeof((xs)[0]))

#define UNUSED(x) ((void)(x))
#define UNREACHABLE() (__builtin_unreachable())

void die(const char *msgf, ...);
void *xcalloc(size_t nmemb, size_t sz);
void *clone(void *p, size_t n);

#endif /* UTIL_H */
