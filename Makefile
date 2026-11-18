# FEAR OF DEAD - PS3 homebrew (PSL1GHT)  -> make pkg
ifeq ($(strip $(PSL1GHT)),)
$(error "Set PSL1GHT (e.g. export PSL1GHT=/usr/local/ps3dev)")
endif
include $(PSL1GHT)/ppu_rules

TARGET    := fear_of_dead
SOURCES   := source
BUILD     := build
TITLE     := Fear of Dead (Trial)
APPID     := FEAR00001
CONTENTID := UP0001-$(APPID)_00-FEAROFDEAD000001
SFOXML    := sfo.xml
ICON0     := ICON0.PNG

CFLAGS    := -O2 -Wall -mcpu=cell $(MACHDEP) -I$(PORTLIBS)/include -I$(PS3DEV)/ppu/include -Isource
LDFLAGS   := $(MACHDEP) -L$(PORTLIBS)/lib -L$(PS3DEV)/ppu/lib
LIBS      := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

CFILES    := $(wildcard $(SOURCES)/*.c)
CFILES    := $(filter-out $(SOURCES)/test_host.c,$(CFILES))
OFILES    := $(patsubst $(SOURCES)/%.c,$(BUILD)/%.o,$(CFILES))

.PHONY: all pkg clean
all: $(BUILD)/$(TARGET).self

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/$(TARGET).self: $(BUILD)/$(TARGET).elf
	$(SELF) $< $@ >/dev/null 2>&1 || $(SELF) $< $@

pkg: $(BUILD)/$(TARGET).self
	@mkdir -p $(BUILD)/pkg/USRDIR
	@cp $(BUILD)/$(TARGET).self $(BUILD)/pkg/USRDIR/EBOOT.BIN
	@cp $(ICON0) $(BUILD)/pkg/ICON0.PNG
	@$(SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) $(BUILD)/pkg/PARAM.SFO
	@$(PKG) --contentid $(CONTENTID) $(BUILD)/pkg/ $(TARGET).pkg
	@echo "Built $(TARGET).pkg"

clean:
	rm -rf $(BUILD) $(TARGET).pkg
