TARGET   = main
TITLE    = Fazenda PS3
APPID    = FARM00001
CONTENTID = UP0001-$(APPID)_00-0000000000000000

CFLAGS   = -O2 -Wall -mcpu=cell -std=gnu99 -I$(PSL1GHT)/ppu/include -I$(PS3DEV)/portlibs/ppu/include
LDFLAGS  = -L$(PSL1GHT)/ppu/lib
LIBS     = -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

OFILES   = main.o
BUILDDIR = build

include $(PSL1GHT)/ppu_rules

$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

pkg: $(TARGET).pkg

clean:
	rm -rf $(TARGET)*.pkg $(TARGET)*.self $(TARGET).elf $(BUILDDIR) $(OFILES)
