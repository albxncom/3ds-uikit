/* The files of sound/ inside the application, as they are: each from its name to name_end (sound_data.h). The
 * application's build tells the assembler to look in the kit's sound/. The menu music's
 * tracks come from the table in tracks.h, so a track is built in by its line there. */
#include "tracks.h"

    .section .rodata.sound, "a", %progbits

    .macro sound name, path
    .balign 4
    .global \name
    .type \name, %object
\name:
    .incbin "\path"
    .global \name\()_end
\name\()_end:
    .size \name, . - \name
    .endm

    sound snd_move, "move.wav"
    sound snd_confirm, "confirm.wav"
    sound snd_back, "back.wav"
    sound snd_popup, "popup.wav"
    sound snd_error, "error.wav"
    sound snd_done, "done.wav"
    sound snd_drop, "drop.wav"
    sound snd_working, "working.bcstm"

#define TRACK_FILE(symbol, file, title, author, licence, page) sound track_##symbol, file ;
    TRACKS(TRACK_FILE)
