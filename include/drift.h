/* The pattern's drift behind the screens (ui_layout.h, PATTERN_*) as a plain number, so the host tests check it: how far
 * the pattern has moved. By the clock: a late frame moves it further at once, it does not slow it. */
#ifndef DRIFT_H
#define DRIFT_H

#include <stdint.h>

#include "ui_layout.h"

/* How far the pattern has moved along an axis it moves on after ms, in px from 0 up to PATTERN_TILE, where it is
 * the same picture again: PATTERN_DRIFT px a second. */
static inline float turn_drift(uint64_t ms)
{
    uint64_t period = (uint64_t)PATTERN_TILE * 1000 / PATTERN_DRIFT;
    return (float)(ms % period) * PATTERN_DRIFT / 1000.0f;
}

#endif
