/* The looks this build holds (looks_built.h, made by make from LOOKS) by their number; Stock for any other. */
#include "look.h"
#include "looks_built.h"

#define HELD(name, NAME) [LOOK_##NAME] = &look_##name,
static const look *const BUILT[LOOK_COUNT] = {LOOKS_BUILT(HELD)};

int look_built(int id)
{
    return id >= 0 && id < LOOK_COUNT && BUILT[id];
}

const look *look_get(int id)
{
    return look_built(id) ? BUILT[id] : &look_stock;
}
