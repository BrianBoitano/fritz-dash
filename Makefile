# Fritz Dash - OpenOrbis PS4 build. Adapted from the toolchain's samples/graphics Makefile.
# Builds out of tree: nothing is written into the source directory.
#   make BUILD=/path/to/build          -> $(BUILD)/$(CONTENT_ID).pkg
#   make BUILD=/path/to/build sim      -> desktop simulator with screenshots
#   make BUILD=/path/to/build SDL2_MINGW=/path/to/SDL2-2.x/x86_64-w64-mingw32 win win-zip
#                                      -> $(BUILD)/win/FritzDash.exe and the Windows zip
#   LEVELDIR=/path/to/maps             -> build other built-in level maps (tests only)

TITLE       := Fritz Dash
VERSION     := 03.20
APP_VER     := 03.20
TITLE_ID    := FRTZ00001
CONTENT_ID  := IV0000-FRTZ00001_00-FRITZDASH0000000

TOOLCHAIN   ?= $(OO_PS4_TOOLCHAIN)
BUILD       ?= build
SRCDIR      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_ABS    = $(abspath $(BUILD))
STAGE        = $(BUILD_ABS)/pkg
TOOLS       := $(TOOLCHAIN)/bin/linux
SAMPLE      := $(TOOLCHAIN)/samples/hello_world

LIBS        := -lc -lkernel -lSceVideoOut -lScePad -lSceUserService -lSceSystemService -lSceAudioOut
GAME_SRCS   := game.c badges.c ghost.c hint.c rate.c endless.c editor.c keyboard.c songs.c icons.c stock.c world.c level.c play.c play_draw.c synth.c draw.c modes.c obj_portals.c transform.c obj_eggs.c eggs.c rewards.c secrets.c lab.c
# Every mode is its own src/mode_<name>.c (docs/MODES.md); they all come along.
MODE_SRCS   := $(notdir $(wildcard $(SRCDIR)/src/mode_*.c))
SRCS        := ps4_main.c $(GAME_SRCS) $(MODE_SRCS)
OBJS         = $(patsubst %.c,$(BUILD_ABS)/obj/%.o,$(SRCS)) $(BUILD_ABS)/obj/builtin_levels.o $(BUILD_ABS)/obj/stock_icons.o \
               $(BUILD_ABS)/obj/lab_levels.o
LEVELDIR    ?= $(SRCDIR)/levels
LEVELS      := $(sort $(wildcard $(LEVELDIR)/*.txt))
SAMPLES     := $(sort $(wildcard $(SRCDIR)/samples/*.txt))
GEN         := $(BUILD_ABS)/gen/builtin_levels.c
GEN_ICONS   := $(BUILD_ABS)/gen/stock_icons.c
# MODE LAB: every mode's demo map, built in (lab.c).
LAB_MAPS    := $(sort $(wildcard $(SRCDIR)/samples/mode_*.txt))
GEN_LAB     := $(BUILD_ABS)/gen/lab_levels.c

CC          := clang
LD          := ld.lld
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -Wall -Wextra \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

PKG_FILES   := eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png \
               sce_sys/pic1.png sce_module/libc.prx sce_module/libSceFios2.prx

.PHONY: all clean sim win win-zip

all: $(BUILD_ABS)/$(CONTENT_ID).pkg

$(BUILD_ABS)/$(CONTENT_ID).pkg: $(STAGE)/pkg.gp4
	cd $(STAGE) && $(TOOLS)/PkgTool.Core pkg_build pkg.gp4 $(BUILD_ABS)

$(STAGE)/pkg.gp4: $(STAGE)/eboot.bin $(STAGE)/sce_sys/param.sfo $(STAGE)/sce_sys/icon0.png
	mkdir -p $(STAGE)/sce_sys/about $(STAGE)/sce_module
	cp $(SAMPLE)/sce_sys/about/right.sprx $(STAGE)/sce_sys/about/right.sprx
	cp $(SAMPLE)/sce_module/libc.prx $(SAMPLE)/sce_module/libSceFios2.prx $(STAGE)/sce_module/
	cd $(STAGE) && $(TOOLS)/create-gp4 -out pkg.gp4 --content-id=$(CONTENT_ID) --files "$(PKG_FILES)"

$(STAGE)/sce_sys/icon0.png: $(SRCDIR)/tools/gen_art.py
	python3 $(SRCDIR)/tools/gen_art.py $(STAGE)/sce_sys

$(STAGE)/sce_sys/param.sfo: $(SRCDIR)/Makefile
	mkdir -p $(STAGE)/sce_sys
	rm -f $@
	$(TOOLS)/PkgTool.Core sfo_new $@
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(APP_VER)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

$(STAGE)/eboot.bin: $(OBJS)
	mkdir -p $(STAGE)
	$(LD) $(OBJS) -o $(BUILD_ABS)/fritzdash.elf $(LDFLAGS)
	$(TOOLS)/create-fself -in=$(BUILD_ABS)/fritzdash.elf -out=$(BUILD_ABS)/fritzdash.oelf \
		--eboot "$(STAGE)/eboot.bin" --paid 0x3800000000000011

$(BUILD_ABS)/obj/%.o: $(SRCDIR)/src/%.c $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(GEN): $(LEVELS) $(SRCDIR)/tools/gen_levels.py
	python3 $(SRCDIR)/tools/gen_levels.py c $@ $(LEVELS)

$(BUILD_ABS)/obj/builtin_levels.o: $(GEN) $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRCDIR)/src -c -o $@ $<

$(GEN_LAB): $(LAB_MAPS) $(SRCDIR)/tools/gen_levels.py
	python3 $(SRCDIR)/tools/gen_levels.py lab $@ $(LAB_MAPS)

$(BUILD_ABS)/obj/lab_levels.o: $(GEN_LAB) $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRCDIR)/src -c -o $@ $<

$(GEN_ICONS): $(SRCDIR)/tools/gen_icons.py $(SRCDIR)/tools/icon_art.py
	python3 $(SRCDIR)/tools/gen_icons.py c $@

$(BUILD_ABS)/obj/stock_icons.o: $(GEN_ICONS) $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRCDIR)/src -c -o $@ $<

$(BUILD_ABS)/samples/.done: $(SAMPLES) $(SRCDIR)/tools/gen_levels.py
	python3 $(SRCDIR)/tools/gen_levels.py json $(BUILD_ABS)/samples $(SAMPLES)
	touch $@

sim: $(BUILD_ABS)/sim $(BUILD_ABS)/samples/.done
	mkdir -p $(BUILD_ABS)/shots
	SIM_TRACES=$(SRCDIR)/tools/traces_base.txt $(BUILD_ABS)/sim $(BUILD_ABS)/shots $(BUILD_ABS)/samples/*.json

SIM_SRCS    := $(SRCDIR)/tools/sim.c $(SRCDIR)/tools/solver.c $(SRCDIR)/tools/traces.c $(addprefix $(SRCDIR)/src/,$(GAME_SRCS) $(MODE_SRCS))
$(BUILD_ABS)/sim: $(SIM_SRCS) $(SRCDIR)/tools/solver.h $(GEN) $(GEN_ICONS) $(GEN_LAB) $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(BUILD_ABS)
	$(CC) -O2 -Wall -Wextra -I$(SRCDIR)/src -o $@ $(SIM_SRCS) $(GEN) $(GEN_ICONS) $(GEN_LAB)

# --- Windows (PC): MinGW-w64 and SDL2's MinGW development files --------------
# The same game code with src/pc_main.c as the platform layer (README.md,
# "Windows (PC)"). SDL2 is linked in, so FritzDash.exe is the whole game.
WIN_CC      ?= x86_64-w64-mingw32-gcc
WIN_WINDRES ?= x86_64-w64-mingw32-windres
SDL2_MINGW  ?=
WIN_VERSION := 3.2
WIN          = $(BUILD_ABS)/win
WIN_CFLAGS   = -O2 -Wall -Wextra -D__USE_MINGW_ANSI_STDIO=1 -DFRITZ_VERSION='"$(VERSION)"' -I$(SDL2_MINGW)/include/SDL2
# SDL2 linked statically: the libraries are what its sdl2-config --static-libs lists.
WIN_LIBS     = -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main $(SDL2_MINGW)/lib/libSDL2.a -lm -ldinput8 -ldxguid -ldxerr8 \
               -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid
WIN_OBJS     = $(patsubst %.c,$(WIN)/obj/%.o,pc_main.c $(GAME_SRCS) $(MODE_SRCS)) $(WIN)/obj/builtin_levels.o \
               $(WIN)/obj/stock_icons.o $(WIN)/obj/lab_levels.o $(WIN)/obj/icon.o
WIN_ZIP      = $(WIN)/FritzDash-win64-$(WIN_VERSION).zip

ifneq ($(filter win win-zip,$(MAKECMDGOALS)),)
ifeq ($(strip $(SDL2_MINGW)),)
$(error SDL2_MINGW must name the SDL2 MinGW package's x86_64-w64-mingw32 folder)
endif
endif

win: $(WIN)/FritzDash.exe

win-zip: $(WIN_ZIP)

$(WIN)/FritzDash.exe: $(WIN_OBJS)
	$(WIN_CC) -mwindows -static -s -o $@ $(WIN_OBJS) $(WIN_LIBS)

$(WIN)/obj/%.o: $(SRCDIR)/src/%.c $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) -c -o $@ $<

$(WIN)/obj/%.o: $(BUILD_ABS)/gen/%.c $(wildcard $(SRCDIR)/src/*.h)
	mkdir -p $(dir $@)
	$(WIN_CC) $(WIN_CFLAGS) -I$(SRCDIR)/src -c -o $@ $<

# The exe's icon (Explorer, the taskbar and the window): icon0.png at the
# sizes Windows asks for.
$(WIN)/fritzdash.ico: $(SRCDIR)/tools/gen_art.py
	mkdir -p $(WIN)/art
	python3 $(SRCDIR)/tools/gen_art.py $(WIN)/art
	python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2], \
		sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])" $(WIN)/art/icon0.png $@

$(WIN)/obj/icon.o: $(WIN)/fritzdash.ico
	mkdir -p $(dir $@)
	printf '1 ICON "fritzdash.ico"\n' > $(WIN)/fritzdash.rc
	$(WIN_WINDRES) -I$(WIN) -O coff -o $@ $(WIN)/fritzdash.rc

$(WIN_ZIP): $(WIN)/FritzDash.exe $(SRCDIR)/docs/README-PC.txt
	rm -rf $(WIN)/zip $@
	mkdir -p $(WIN)/zip
	cp $(WIN)/FritzDash.exe $(WIN)/zip/
	sed 's/$$/\r/' $(SRCDIR)/docs/README-PC.txt > $(WIN)/zip/README-PC.txt
	cd $(WIN)/zip && zip -X -9 $@ FritzDash.exe README-PC.txt

clean:
	rm -rf $(BUILD_ABS)
