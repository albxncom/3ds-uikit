/* The menu music the application is built with, in one table: a line a track, the first one the default.
 *
 *     X(symbol, file in sound/, title, author, licence as taken, source page)
 *
 * Adding or dropping a track is its line here, its file in sound/ and its section in sound/CREDITS.md;
 * make test holds the three against each other. The settings screen lists the tracks in this order and the
 * settings file names the chosen one by its file, so a line may move or go without another track's choice changing. */
#ifndef TRACKS_H
#define TRACKS_H

#define TRACKS(X) \
    X(somewhere_in_the_elevator, "somewhere-in-the-elevator.bcstm", "Somewhere in the Elevator", "Peachtea", \
      "CC0, one of the four licences its page offers", "https://opengameart.org/content/somewhere-in-the-elevator") \
    X(urban_shop, "urban-shop.bcstm", "Urban Shop", "MintoDog", "CC0", "https://opengameart.org/content/urban-shop")

#ifndef __ASSEMBLER__           /* sound_data.S takes the table alone */

typedef struct track {
    const char *file, *title, *author, *licence, *page;
} track;

#define TRACK_PLACE(symbol, file, title, author, licence, page) TRACK_##symbol,
enum { TRACKS(TRACK_PLACE) TRACK_COUNT };
#define TRACK_DEFAULT 0

extern const track TRACK_LIST[TRACK_COUNT];

/* The track with this file name: its place in the list, or -1. */
int track_find(const char *file);

#endif
#endif
