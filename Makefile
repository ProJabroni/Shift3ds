#---------------------------------------------------------------------------------
# Shift 3DS - build with devkitARM + libctru + citro2d (devkitPro "3ds-dev").
#   make        -> shift3ds.3dsx   (Homebrew Launcher)
#   make cia    -> shift3ds.cia    (install with FBI; needs makerom + bannertool in PATH)
#   make clean
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM, e.g. export DEVKITARM=/opt/devkitpro/devkitARM")
endif

include $(DEVKITARM)/3ds_rules

# Recipe lines start with ">" instead of a Tab, so this file survives copy/paste.
.RECIPEPREFIX := >

# devkitARM's compile rule writes dependency files here; it must not be empty.
DEPSDIR := .

TARGET      := shift3ds
SOURCES     := $(wildcard source/*.c)
OFILES      := $(SOURCES:.c=.o)

APP_TITLE   := Shift 3DS
APP_LONG    := Shift the world upside-down
APP_AUTHOR  := You

MAKEROM     ?= makerom
BANNERTOOL  ?= bannertool

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations -ffunction-sections -fdata-sections \
               $(ARCH) -D__3DS__ -I$(CTRULIB)/include -I$(PORTLIBS)/include
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(TARGET).map
LIBS        := -lcitro2d -lcitro3d -lctru -lm
LIBPATHS    := -L$(CTRULIB)/lib -L$(PORTLIBS)/lib

.PHONY: all cia clean

all: $(TARGET).3dsx

cia: $(TARGET).cia

%.o: %.c source/game.h source/atari_data.h source/music.h
> $(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
> $(CC) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

# The .icn produced by bannertool is a standard SMDH, so it works for the 3dsx too.
$(TARGET).icn: assets/icon.png
> $(BANNERTOOL) makesmdh -s "$(APP_TITLE)" -l "$(APP_LONG)" -p "$(APP_AUTHOR)" -i $< -o $@

$(TARGET).bnr: assets/banner.png assets/audio.wav
> $(BANNERTOOL) makebanner -i assets/banner.png -a assets/audio.wav -o $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).icn
> 3dsxtool $< $@ --smdh=$(TARGET).icn

$(TARGET).cia: $(TARGET).elf $(TARGET).bnr $(TARGET).icn shift3ds.rsf
> $(MAKEROM) -f cia -o $@ -elf $(TARGET).elf -rsf shift3ds.rsf -icon $(TARGET).icn -banner $(TARGET).bnr -exefslogo -target t

clean:
> rm -f source/*.o $(TARGET).elf $(TARGET).3dsx $(TARGET).cia $(TARGET).bnr $(TARGET).icn $(TARGET).map
