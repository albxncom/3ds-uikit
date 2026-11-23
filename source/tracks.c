#include "tracks.h"

#include <string.h>

#define TRACK_ENTRY(symbol, file, title, author, licence, page) {file, title, author, licence, page},
const track TRACK_LIST[TRACK_COUNT] = {TRACKS(TRACK_ENTRY)};

int track_find(const char *file)
{
    for (int i = 0; i < TRACK_COUNT; i++)
        if (!strcmp(TRACK_LIST[i].file, file))
            return i;
    return -1;
}
