/* The checks of a host test program: CHECK and CHECK_EQ count each check and print every failed one with its place;
 * check_summary prints the program's line, "NAME: N checks, F failed", and gives its exit code. */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long a_ = (long long)(a), b_ = (long long)(b); checks++; if (a_ != b_) { failures++; fprintf(stderr, "%s:%d: %s = %lld, want %lld\n", __FILE__, __LINE__, #a, a_, b_); } } while (0)

static inline int check_summary(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, checks, failures);
    return failures != 0;
}

#endif
