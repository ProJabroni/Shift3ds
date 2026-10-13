#---------------------------------------------------------------------------------
# DSP1 (zoogie, MIT licence) - builds the DSP firmware dumper.
#   make        -> dsp1.3dsx
#   make cia    -> dsp1.cia
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM")
endif
include $(DEVKITARM)/3ds_rules
.RECIPEPREFIX := >
DEPSDIR := .

TARGET   := dsp1
SOURCES  := $(wildcard source/*.c)
OFILES   := $(SOURCES:.c=.o)
MAKEROM  ?= makerom

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections -fdata-sections \
            $(ARCH) -D__3DS__ -I$(CTRULIB)/include
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections
LIBS     := -lctru -lm
LIBPATHS := -L$(CTRULIB)/lib

.PHONY: all cia clean
all: $(TARGET).3dsx
cia: $(TARGET).cia

%.o: %.c
> $(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
> $(CC) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

$(TARGET).3dsx: $(TARGET).elf
> 3dsxtool $< $@ --smdh=cia/icon.icn

$(TARGET).cia: $(TARGET).elf
> $(MAKEROM) -f cia -o $@ -rsf $(TARGET).rsf -target t -elf $(TARGET).elf -icon cia/icon.icn -banner cia/banner.bnr -exefslogo

clean:
> rm -f source/*.o $(TARGET).elf $(TARGET).3dsx $(TARGET).cia
