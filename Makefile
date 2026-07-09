# ============================================================================
#  CART CHAOS 3D - Nintendo 3DS homebrew build (devkitARM / libctru / citro2d)
#
#  Requires devkitARM (https://devkitpro.org) with the `3ds-dev` package set:
#      (dkp-)pacman -S 3ds-dev
#
#  Build on a PC/Mac/Linux, then copy the output to your homebrewed 3DS:
#      make
#      # -> cartchaos.3dsx  cartchaos.smdh  (copy both to /3ds/CartChaos/)
#
#  Controls:
#      A = sprint / hop into cart / restart | D-Pad = lean + steer
#      CirclePad = steer | B = drink for energy | START = back / restart
# ============================================================================

TARGET      := cartchaos
BUILD       := build
SOURCES     := source/main.cpp
DATA        :=
INCLUDES    :=
LIBS        := -lctru -lcitro2d -lctrud -lndsp

ICON        := resources/icon.png
SMDHTOOL    := smdhtool
TITLE       := Cart Chaos 3D
DESC        := downhill shopping-cart mayhem
AUTHOR      := OpenClaude

# devkitARM toolchain (env vars are set by the devkitpro container;
# defaults below cover local / CI use).
DEVKITPRO  ?= /opt/devkitpro
DEVKITARM  ?= $(DEVKITPRO)/devkitARM
CTRULIB    ?= $(DEVKITPRO)/libctru
CITRO2D    ?= $(DEVKITPRO)/citro2d

export PATH := $(DEVKITARM)/bin:$(PATH)
CC          := $(DEVKITARM)/bin/arm-none-eabi-g++
OBJCOPY     := $(DEVKITARM)/bin/arm-none-eabi-objcopy
STRIP       := $(DEVKITARM)/bin/arm-none-eabi-strip

CFLAGS  := -Wall -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard \
           -mfpu=vfpv2 -ffast-math -fno-rtti -std=gnu++17 \
           -I$(CTRULIB)/include -I$(CITRO2D)/include $(INCLUDES)
CXXFLAGS:= $(CFLAGS)
LDFLAGS := -specs=3dsx.specs -g $(LIBS) \
           -L$(CTRULIB)/lib -L$(CITRO2D)/lib

# Auto-generated dependency list
OFILES := $(SOURCES:%.cpp=$(BUILD)/%.o)

.PHONY: all clean cia

all: $(BUILD)/$(TARGET).3dsx

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP -MF $(BUILD)/$*.d $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) -o $@

# Generate the home-menu icon metadata from the PNG.
$(BUILD)/$(TARGET).smdh: $(ICON)
	@mkdir -p $(dir $@)
	$(SMDHTOOL) --create "$(TITLE)" "$(DESC)" "$(AUTHOR)" "$(ICON)" "$@"

$(BUILD)/$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).smdh
	3dsxtool $< $@ --smdh="$(BUILD)/$(TARGET).smdh"

# Optional: produce a .cia installer (needs makerom, present in the
# devkitpro/devkitarm image). Harmless to skip if makerom is absent.
cia: $(BUILD)/$(TARGET).3dsx
	@command -v makerom >/dev/null 2>&1 && \
	  makerom -f cia -o $(TARGET).cia -target t -exefslogo \
	    -elf $(BUILD)/$(TARGET).elf -icon $(BUILD)/$(TARGET).smdh \
	    -banner $(BUILD)/$(TARGET).smdh || \
	  echo "makerom not found; skipping .cia (3dsx is enough)."

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia

-include $(OFILES:.o=.d)
