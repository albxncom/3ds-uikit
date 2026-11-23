# The kit's example for the 3DS, built on the kit alone. Runs in the devkitPro image:
#   make         build/3ds-uikit-example.3dsx (and the .elf and .smdh it is made of)
#   make clean   removes build/ (with UIKIT set: build/installed/ alone)
# The kit is the folder above: every one of its .c files (source/), its art drawn by its own generator (art/art.py
# without --app, for every look whose source/look_<name>.c is there, Stock always) and its sounds linked in as they
# are (source/sound_data.S), with its headers (include/) on the include path.
# Or the kit as installed (make install in the folder above, or the package): make UIKIT=PREFIX, PREFIX the folder it
# was installed into (devkitPro's $(PORTLIBS), or a staging folder's copy of it); then only this folder's .c files are
# compiled, against PREFIX/include/3ds-uikit, and linked with PREFIX/lib/lib3ds-uikit.a, into build/installed/ (its
# own folder, so that one build never stands in for the other); nothing of the folder above is read.
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set: run this in the devkitPro image)
endif
include $(DEVKITARM)/3ds_rules

KIT     := ..
UIKIT   ?=
NAME    := 3ds-uikit-example
TITLE   := 3ds-uikit example
DESCRIPTION := The UI kit's elements, a page each
AUTHOR  := 3ds-uikit
BUILD   := $(if $(strip $(UIKIT)),build/installed,build)
ARTDIR  := $(BUILD)/art
OUT     := $(BUILD)/$(NAME)

ifeq ($(strip $(UIKIT)),)
LOOKS   := $(foreach n,stock frost night prism,$(if $(wildcard $(KIT)/source/look_$(n).c),$(n)))
LOOKS_X := $(foreach n,$(LOOKS),X($(n), $(shell echo $(n) | tr a-z A-Z)))
LOOKS_H := /* make: the looks this build holds (look.h) */\n\#define LOOKS_HELD $(words $(LOOKS))
LOOKS_H += \n\#define LOOKS_BUILT(X) $(LOOKS_X)\n
ART     := $(ARTDIR)/ui_art.c $(ARTDIR)/ui_art.h $(patsubst %,$(ARTDIR)/art_%.c,$(LOOKS))
ART_PY  := $(KIT)/art/art.py $(KIT)/art/schemes.py $(KIT)/art/liquid.py $(KIT)/include/ui_layout.h \
           $(KIT)/include/popup_band.h $(wildcard $(KIT)/icons/*.svg)
SOURCES := $(wildcard *.c) $(notdir $(wildcard $(KIT)/source/*.c))
SOUNDS  := $(wildcard $(KIT)/sound/*.wav) $(wildcard $(KIT)/sound/*.bcstm)
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o)) $(BUILD)/ui_art.o $(patsubst %,$(BUILD)/art_%.o,$(LOOKS)) \
           $(BUILD)/sound_data.o
vpath %.c . $(KIT)/source
INCLUDE := -I$(KIT)/include -I$(ARTDIR)
else
SOURCES := $(wildcard *.c)
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o))
INCLUDE := -I$(UIKIT)/include/3ds-uikit
endif

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
BASE    := -std=gnu11 -O2 -g -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ \
           -isystem $(CTRULIB)/include -isystem $(PORTLIBS)/include
CFLAGS  := $(BASE) -Wall -Wextra -Werror -Wshadow -Wvla -I. $(INCLUDE)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(OUT).map $(if $(strip $(UIKIT)),-L$(UIKIT)/lib) -L$(CTRULIB)/lib \
           -L$(PORTLIBS)/lib
LIBS    := $(if $(strip $(UIKIT)),-l3ds-uikit) -lcitro2d -lcitro3d -lz -lctru -lm

.PHONY: all clean FORCE
all: $(OUT).3dsx

$(BUILD) $(ARTDIR):
	@mkdir -p $@

ifeq ($(strip $(UIKIT)),)
$(BUILD)/%.o: %.c | $(ARTDIR)/ui_art.h $(ARTDIR)/looks_built.h $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/ui_art.o $(patsubst %,$(BUILD)/art_%.o,$(LOOKS)): $(BUILD)/%.o: $(ARTDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# written anew only when it would change (the objects that include it follow through their .d files)
$(ARTDIR)/looks_built.h: FORCE | $(ARTDIR)
	@printf '$(LOOKS_H)' | cmp -s - $@ || printf '$(LOOKS_H)' > $@

$(BUILD)/sound_data.o: $(KIT)/source/sound_data.S $(KIT)/include/tracks.h $(SOUNDS) | $(BUILD)
	$(CC) $(ARCH) -I$(KIT)/include -Wa,-I$(KIT)/sound -c $< -o $@

# each look's part of the kit's art, then their join; no --app: the example brings no images of its own
.SECONDEXPANSION:
$(ARTDIR)/%.part: $(KIT)/art/look_%.py $$(wildcard $(KIT)/include/look_$$*.h) $(ART_PY) | $(ARTDIR)
	python3 -I $(KIT)/art/art.py --look $* --part $@

$(ART) &: $(patsubst %,$(ARTDIR)/%.part,$(LOOKS)) $(ART_PY)
	python3 -I $(KIT)/art/art.py --join $(ARTDIR) $(LOOKS)
else
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
endif

$(OUT).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

# libctru's own icon
$(OUT).smdh: $(CTRULIB)/default_icon.png Makefile | $(BUILD)
	smdhtool --create "$(TITLE)" "$(DESCRIPTION)" "$(AUTHOR)" $< $@

$(OUT).3dsx: $(OUT).elf $(OUT).smdh
	3dsxtool $< $@ --smdh=$(OUT).smdh

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
