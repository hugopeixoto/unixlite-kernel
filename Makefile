export TOPDIR := $(shell pwd)
export DEBUG := 0

all: Image

# parameters passed to Rules.make
FULLLINK := 1
LDFLAGS2 := -Ttext 0xc0100000
SUBDIRS  := init lib asm mm fs dev kern net
include	$(TOPDIR)/Rules.make

COLLECT := $(TOPDIR)/boot/collect.sh
GLUE 	:= $(TOPDIR)/boot/glue
BOOTSECT:= $(TOPDIR)/boot/bootsect
SETUP 	:= $(TOPDIR)/boot/setup

Image: $(TARGET)
	@make -C boot
	(cd boot; $(COLLECT) $(TARGET))
	$(GLUE) $(BOOTSECT) $(SETUP) $(TARGET) >Image
	$(TOPDIR)/boot/bootparam Image /dev/hda1

floppy: Image
	dd if=Image of=/dev/fd0H1440 bs=64k 

etags:
	find . -name '*.*[ch]' | etags -


include $(TOPDIR)/Makefile.dbg
