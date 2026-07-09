# ============================================================================
#  CART CHAOS 3D - Nintendo 3DS homebrew build (devkitARM / libctru / citro2d)
#
#  Requires devkitARM (https://devkitpro.org) with the `3ds-dev` package set:
#      (dkp-)pacman -S 3ds-dev
#
#  Build on a PC/Mac/Linux (or in GitHub Actions via devkitpro/devkitarm),
#  then copy the output to your homebrewed 3DS:
#      make
#      # -> build/cartchaos.3dsx  build/cartchaos.smdh  (copy both to /3ds/CartChaos/)
# ============================================================================

# --- devkitPro toolchain location (env vars set by the container) ----------
DEVKITPRO  ?= /opt/devkitpro
DEVKITARM  ?= $(DEVKITPRO)/devkitARM
PORTLIBS   ?= $(DEVKITPRO)/portlibs/3ds

# Pull in the canonical 3DS build rules (ARCH, CFLAGS, LDFLAGS, library paths,
# and the correct -specs=3dsx.specs handling). This is what the
# devkitpro/devkitarm image expects.
include $(DEVKITARM)/3ds_rules

TARGET      := cartchaos
BUILD       := build
SOURCES     := source/main.cpp
DATA        :=
INCLUDES    := -I$(DEVKITPRO)/libctru/include -I$(DEVKITPRO)/citro2d/include

# Link the homebrew system libs plus the portlibs (ndsp audio, ctrud services).
LIBS        := -lctru -lcitro2d -lctrud -lndsp \
               -L$(DEVKITPRO)/portlibs/3ds/lib

ICON        := resources/icon.png
SMDHTOOL    := smdhtool
TITLE       := Cart Chaos 3D
DESC        := downhill shopping-cart mayhem
AUTHOR      := OpenClaude

export PATH := $(DEVKITARM)/bin:$(PATH)
CC          := $(DEVKITARM)/bin/arm-none-eabi-g++

# Auto-generated dependency list
OFILES := $(SOURCES:%.cpp=$(BUILD)/%.o)

.PHONY: all clean cia

all: $(BUILD)/$(TARGET).3dsx

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) -MMD -MP -MF $(BUILD)/$*.d $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# Generate the home-menu icon metadata from the PNG.
$(BUILD)/$(TARGET).smdh: $(ICON)
	@mkdir -p $(dir $@)
	$(SMDHTOOL) --create "$(TITLE)" "$(DESC)" "$(AUTHOR)" "$(ICON)" "$@"

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
