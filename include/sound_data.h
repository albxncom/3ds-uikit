/* The files of sound/ as they lie in the application (sound_data.S): the bytes from name to name_end. */
#ifndef SOUND_DATA_H
#define SOUND_DATA_H

#include <stdint.h>

#include "tracks.h"

extern const uint8_t snd_move[], snd_move_end[];
extern const uint8_t snd_confirm[], snd_confirm_end[];
extern const uint8_t snd_back[], snd_back_end[];
extern const uint8_t snd_popup[], snd_popup_end[];
extern const uint8_t snd_error[], snd_error_end[];
extern const uint8_t snd_done[], snd_done_end[];
extern const uint8_t snd_drop[], snd_drop_end[];
extern const uint8_t snd_working[], snd_working_end[];

/* the menu music: track_<symbol> for every line of tracks.h */
#define TRACK_EXTERN(symbol, file, title, author, licence, page) \
    extern const uint8_t track_##symbol[], track_##symbol##_end[];
TRACKS(TRACK_EXTERN)

#endif
