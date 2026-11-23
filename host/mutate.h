/* The mutation runs' random numbers (xorshift32, one sequence for the whole program) and the exact-size copies
 * the host tests read from, so that the asan build catches a read past a buffer's end. */
#ifndef MUTATE_H
#define MUTATE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng = 0x2545F491u;

static uint32_t next_rand(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint8_t *exact_copy(const uint8_t *p, size_t n)
{
    uint8_t *q = malloc(n ? n : 1);
    if (!q) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
    memcpy(q, p, n);
    return q;
}

#endif
