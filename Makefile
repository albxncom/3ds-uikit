# The kit's own build; every path is the folder's own: the headers in include/, the sources in source/, the art
# generator in art/.
# host:          the kit's unit tests (build/host/): test_kit (popup.c, scroll.c, touch.c, wrap.c, order.h, ease.h),
#                test_sound (bcstm.c, wav.c, tracks.c, work.c and the files of sound/: build/host/test_sound sound) and
#                test_probe (probe.h)
# asan:          test_kit and test_sound with AddressSanitizer and UBSan (build/asan/)
# art:           the kit's art made alone (art/art.py without --app, every look held, Stock always) and
#                looks_built.h, in build/art/
# test:          all of it, then the tests run: the unit tests in both builds, and the Python tests of host/
# example:       the example, example/build/3ds-uikit-example.3dsx; run inside the devkitPro image
#                (tools/build/image.sh names it)
# docs-pictures: the pictures of the docs, docs/img/, drawn from the example's host walk
# lib:           the kit as a static library for the 3DS, build/3ds/lib3ds-uikit.a: every .c of source/, its art for
#                every look held (as make art) and its sounds (source/sound_data.S), compiled as the example compiles
#                them; run inside the devkitPro image
# install:       lib, then into $(DESTDIR)$(prefix), prefix by default devkitPro's 3DS portlibs folder (3ds_rules'
#                PORTLIBS, /opt/devkitpro/portlibs/3ds in the image): lib/lib3ds-uikit.a; include/3ds-uikit/ every
#                header of the kit (include/) and the two generated ones (ui_art.h, looks_built.h); share/3ds-uikit/ the
#                art generator with what it reads (an application's own part, art/art.py --app), in the repository's
#                layout: art/ its modules, include/ the headers it reads, icons/ with their LICENSE;
#                licenses/3ds-uikit/ the kit's LICENSE, icons/LICENSE and sound/CREDITS.md (the sounds are in the
#                library); nothing outside $(DESTDIR) when it is set. prefix, and DESTDIR when set, must be absolute
#                paths without a space. The docs stay in the repository (the 3DS port packages install none)
# clean:         removes build/ and example/build/

HOST_CC ?= cc
WARN    := -std=c11 -Wall -Wextra -Werror -Wshadow -Wvla
SAN     := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
ART     := build/art

LOOKS   := $(foreach n,stock frost night prism,$(if $(wildcard source/look_$(n).c),$(n)))
LOOKS_X := $(foreach n,$(LOOKS),X($(n), $(shell echo $(n) | tr a-z A-Z)))
LOOKS_H := /* make: the looks this build holds (look.h) */\n\#define LOOKS_HELD $(words $(LOOKS))
LOOKS_H += \n\#define LOOKS_BUILT(X) $(LOOKS_X)\n
ICONS   := $(wildcard icons/*.svg)

KIT_TEST_SRC   := source/popup.c source/scroll.c source/touch.c source/wrap.c
KIT_TEST_HDR   := $(KIT_TEST_SRC:source/%.c=include/%.h) include/order.h include/ease.h include/ui_layout.h \
                  include/popup_band.h host/check.h host/ui_fakes.h
SOUND_TEST_SRC := source/bcstm.c source/wav.c source/tracks.c source/work.c
SOUND_TEST_HDR := $(SOUND_TEST_SRC:source/%.c=include/%.h) include/audio.h include/ui_layout.h include/popup_band.h \
                  host/check.h host/mutate.h host/synth_bcstm.h

# the library's build: devkitPro's own rules (CC, AR, CTRULIB, PORTLIBS), only where the environment has devkitARM
ifneq ($(strip $(DEVKITARM)),)
include $(DEVKITARM)/3ds_rules
prefix  ?= $(PORTLIBS)
endif
# make install stops before anything is built unless prefix, and DESTDIR when set, are absolute paths without a space:
# a relative one would land beside the kit or run into DESTDIR, a space would split a folder into two
ifneq ($(filter install,$(MAKECMDGOALS)),)
$(if $(prefix),,$(error no prefix: run make install in the devkitPro image, or give prefix=FOLDER))
$(foreach v,prefix DESTDIR,$(if $(value $(v)),$(if $(and $(filter /%,$($(v))),$(if $(word 2,$($(v))),,1)),,\
  $(error $(v)=$($(v)): give an absolute path without a space))))
endif
LIB_DIR := build/3ds
LIB     := $(LIB_DIR)/lib3ds-uikit.a
LIB_ART := $(ART)/ui_art.c $(patsubst %,$(ART)/art_%.c,$(LOOKS))
LIB_OBJ := $(patsubst source/%.c,$(LIB_DIR)/%.o,$(wildcard source/*.c)) \
           $(patsubst $(ART)/%.c,$(LIB_DIR)/%.o,$(LIB_ART)) $(LIB_DIR)/sound_data.o
ARCH_3DS := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
# -ffile-prefix-map: the build folder's path is left out of the library (its debug lines name the files from here)
CFLAGS_3DS := -std=gnu11 -O2 -g -ffile-prefix-map=$(CURDIR)=. -mword-relocations -ffunction-sections $(ARCH_3DS) \
              -D__3DS__ -isystem $(CTRULIB)/include -isystem $(PORTLIBS)/include -Wall -Wextra -Werror -Wshadow -Wvla \
              -Iinclude -I$(ART)
INC_DIR   = $(DESTDIR)$(prefix)/include/3ds-uikit
SHARE_DIR = $(DESTDIR)$(prefix)/share/3ds-uikit
LIC_DIR   = $(DESTDIR)$(prefix)/licenses/3ds-uikit

.PHONY: host asan art test example docs-pictures lib install clean

host: build/host/test_kit build/host/test_sound build/host/test_probe
asan: build/asan/test_kit build/asan/test_sound
art: $(ART)/ui_art.h

build/host/test_kit: host/test_kit.c $(KIT_TEST_SRC) $(KIT_TEST_HDR)
	@mkdir -p build/host
	$(HOST_CC) $(WARN) -O2 -Iinclude -o $@ host/test_kit.c $(KIT_TEST_SRC) -lm

build/asan/test_kit: host/test_kit.c $(KIT_TEST_SRC) $(KIT_TEST_HDR)
	@mkdir -p build/asan
	$(HOST_CC) $(WARN) $(SAN) -Iinclude -o $@ host/test_kit.c $(KIT_TEST_SRC) -lm

build/host/test_sound: host/test_sound.c $(SOUND_TEST_SRC) $(SOUND_TEST_HDR)
	@mkdir -p build/host
	$(HOST_CC) $(WARN) -O2 -Iinclude -o $@ host/test_sound.c $(SOUND_TEST_SRC) -lm

build/asan/test_sound: host/test_sound.c $(SOUND_TEST_SRC) $(SOUND_TEST_HDR)
	@mkdir -p build/asan
	$(HOST_CC) $(WARN) $(SAN) -Iinclude -o $@ host/test_sound.c $(SOUND_TEST_SRC) -lm

# the flicker probe's comparison of still frames (probe.h)
build/host/test_probe: host/test_probe.c include/probe.h
	@mkdir -p build/host
	$(HOST_CC) $(WARN) -O2 -Iinclude -o $@ host/test_probe.c -lz

# each look's part of the kit's art, then their join
$(ART)/%.part: art/art.py art/look_%.py $(wildcard include/look_*.h) art/schemes.py art/liquid.py include/ui_layout.h \
               include/popup_band.h $(ICONS)
	@mkdir -p $(ART)
	python3 -I art/art.py --look $* --part $@
$(ART)/ui_art.h: $(patsubst %,$(ART)/%.part,$(LOOKS)) art/art.py art/schemes.py art/liquid.py include/ui_layout.h \
                 include/popup_band.h
	python3 -I art/art.py --join $(ART) $(LOOKS)
	printf '$(LOOKS_H)' > $(ART)/looks_built.h

# test_kit_bounds.py: the kit's files include only each other and the generated headers, each source compiles alone,
# no file names a path outside the folder; test_art_alone.py: every look's art made with no --app; test_art_part.py:
# the generator's hook on small parts written by the test; test_example.py: the example holds every element and
# compiles against the kit alone, and its host walk passes every page; test_kit_docs.py: the docs name every
# function, sound and icon, every link resolves inside the folder, no retired wording
test: host asan $(ART)/ui_art.h
	build/host/test_kit
	build/asan/test_kit
	build/host/test_sound sound
	build/asan/test_sound sound
	build/host/test_probe
	python3 -I host/test_kit_bounds.py $(ART)
	python3 -I host/test_art_alone.py $(ART)
	python3 -I host/test_art_part.py $(ART)
	python3 -I host/test_example.py $(ART)
	python3 -I host/test_kit_docs.py

example:
	$(MAKE) -C example

lib: $(LIB)

$(LIB_DIR)/%.o: source/%.c | $(ART)/ui_art.h
	$(if $(DEVKITARM),,$(error DEVKITARM is not set: run make lib in the devkitPro image))
	@mkdir -p $(LIB_DIR)
	$(CC) $(CFLAGS_3DS) -MMD -MP -c $< -o $@

$(LIB_DIR)/%.o: $(ART)/%.c
	@mkdir -p $(LIB_DIR)
	$(CC) $(CFLAGS_3DS) -MMD -MP -c $< -o $@

# written by the join with ui_art.h
$(LIB_ART): $(ART)/ui_art.h ;

$(LIB_DIR)/sound_data.o: source/sound_data.S include/tracks.h $(wildcard sound/*.wav sound/*.bcstm)
	@mkdir -p $(LIB_DIR)
	$(CC) $(ARCH_3DS) -Iinclude -Wa,-Isound -c $< -o $@

# D: no dates, owners or modes in the archive, so the same objects give the same bytes
$(LIB): $(LIB_OBJ)
	@rm -f $@
	$(AR) rcsD $@ $^

install: $(LIB)
	install -d $(DESTDIR)$(prefix)/lib $(INC_DIR) $(SHARE_DIR)/art $(SHARE_DIR)/include $(SHARE_DIR)/icons \
	  $(LIC_DIR)/icons $(LIC_DIR)/sound
	install -m 644 $(LIB) $(DESTDIR)$(prefix)/lib
	install -m 644 $(wildcard include/*.h) $(ART)/ui_art.h $(ART)/looks_built.h $(INC_DIR)
	install -m 644 $(wildcard art/*.py) $(SHARE_DIR)/art
	install -m 644 include/ui_layout.h include/popup_band.h $(wildcard include/look_*.h) $(SHARE_DIR)/include
	install -m 644 $(ICONS) icons/LICENSE $(SHARE_DIR)/icons
	install -m 644 LICENSE $(LIC_DIR)
	install -m 644 icons/LICENSE $(LIC_DIR)/icons
	install -m 644 sound/CREDITS.md $(LIC_DIR)/sound

# not part of make test, which checks the pictures' files only (test_kit_docs.py)
docs-pictures: $(ART)/ui_art.h
	python3 -I -B tools/flows/kit_pictures.py $(ART) docs/img

clean:
	rm -rf build example/build

-include $(LIB_OBJ:.o=.d)
