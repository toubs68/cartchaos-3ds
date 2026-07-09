# ============================================================================
#  CART CHAOS 3D - Nintendo 3DS homebrew build (devkitARM / libctru / citro2d)
#
#  Requires devkitARM (https://devkitpro.org) with the `3ds-dev` package set:
#      (dkp-)pacman -S 3ds-dev
#
#  Build then copy the output to your homebrewed 3DS:
#      make
#      # -> build/cartchaos.3dsx  build/cartchaos.smdh  (copy both to /3ds/CartChaos/)
# ============================================================================

TARGET      := cartchaos
BUILD       := build
SOURCES     := source/main.cpp
DATA        :=
INCLUDES    :=

# Title metadata for the home-menu icon (.smdh)
APP_TITLE       := Cart Chaos 3D
APP_DESCRIPTION := downhill shopping-cart mayhem
APP_AUTHOR      := OpenClaude
APP_ICON        := resources/icon.png

# --- devkitPro toolchain location (env vars set by the devkitpro container) ---
DEVKITPRO  ?= /opt/devkitpro
DEVKITARM  ?= $(DEVKITPRO)/devkitARM
CTRULIB     ?= $(DEVKITPRO)/libctru
CITRO2D     ?= $(DEVKITPRO)/citro2d
PORTLIBS    ?= $(DEVKITPRO)/portlibs/3ds

export PATH := $(DEVKITARM)/bin:$(PATH)
CC          := $(DEVKITARM)/bin/arm-none-eabi-g++
OBJCOPY     := $(DEVKITARM)/bin/arm-none-eabi-objcopy

# Canonical 3DS arch flags. -mfpu=vfpv2 selects the armv6k/fpu multilib.
CFLAGS  := -Wall -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard \
           -mfpu=vfpv2 -ffast-math -fno-rtti -std=gnu++17 \
           -I$(CTRULIB)/include -I$(CITRO2D)/include $(INCLUDES)
CXXFLAGS := $(CFLAGS)

# 3DS entry point + linker script live here (absolutely pathed to bypass the
# startfile search, which ignores -L). The -L paths cover libctru/citro2d/
# portlibs; ndsp/ctrud are provided by libctru.
# Entry point + linker script resolved by cartchaos.specs (see that file).
# ARCH flags MUST be on the link line too, so the final ELF is tagged
# hard-float (VFP) and the matching crti/crtbegin multilib is selected —
# otherwise the ELF defaults to soft-float and mismatches libctru.
LDSCRIPT := $(DEVKITARM)/arm-none-eabi/lib/3dsx.ld
# NB: do NOT add -L$(DEVKITARM)/arm-none-eabi/lib here — GCC already knows
# its own lib dir and applies the armv6k/fpu (hard-float) multilib from the
# ARCH flags above. Overriding it forces the soft-float libstdc++/libc.
LDFLAGS := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfpv2 \
           -specs=cartchaos.specs -g \
           -L$(CTRULIB)/lib -L$(CITRO2D)/lib -L$(PORTLIBS)/lib

# citro2d is a layer over citro3d; the 3DS hard-float libs provide ndsp/ctrud too.
LIBS    := -lctru -lcitro2d -lcitro3d -lctrud -lm

SMDHTOOL    := smdhtool

OFILES := $(SOURCES:%.cpp=$(BUILD)/%.o)

.PHONY: all clean cia

all: $(BUILD)/$(TARGET).3dsx

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP -MF $(BUILD)/$*.d $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/$(TARGET).smdh: $(APP_ICON)
	@mkdir -p $(dir $@)
	$(SMDHTOOL) --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" "$(APP_ICON)" "$@"

$(BUILD)/$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).smdh
	3dsxtool $< $@ --smdh="$(BUILD)/$(TARGET).smdh"

# Optional: produce a .cia installer (needs makerom, part of 3ds-tools).
cia: $(BUILD)/$(TARGET).3dsx
	@command -v makerom >/dev/null 2>&1 && \
	  makerom -f cia -o $(TARGET).cia -target t -exefslogo \
	    -elf $(BUILD)/$(TARGET).elf -icon $(BUILD)/$(TARGET).smdh \
	    -banner $(BUILD)/$(TARGET).smdh || \
	  echo "makerom not found; skipping .cia (3dsx is enough)."

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia

-include $(OFILES:.o=.d)
