INCLUDE  := -nostdinc -I$(TOPDIR)
CPP  	 := cpp
CPPFLAGS := -M $(INCLUDE) 

LD 	:= ld
ifndef FULLLINK
LDFLAGS	:= -nostdlib -r
endif

CXXFLAGS := -c $(INCLUDE) -Wall  -Wnon-virtual-dtor -Wno-parentheses \
	-Wno-pmf-conversions -Wno-pointer-arith  -Wno-unused-function \
	-Wundef -fno-rtti -Wno-invalid-offsetof \
	-fno-exceptions -fcheck-new -nostdlib -fno-builtin

ifeq ($(DEBUG),1)
CXXFLAGS := $(CXXFLAGS) -g -O 
else
CXXFLAGS := $(CXXFLAGS) -g -fomit-frame-pointer -O
endif

AS 	:= $(CXX) -c
ASFLAGS := $(INCLUDE) -D__ASSEMBLY__

PWD 	:= $(shell pwd)
TARGET 	:= $(PWD)/$(shell basename $(PWD)).target
SRCS 	:= $(wildcard *.S *.cc)
OBJS 	:= $(subst .S,.o,$(subst .cc,.o,$(SRCS)))

PWD	:= $(shell pwd)
TARGET	:= $(PWD)/$(shell basename $(PWD)).target
SRCS	:= $(wildcard *.S *.cc)
OBJS	:= $(patsubst %.cc,%.o,$(patsubst %.S,%.o,$(SRCS)))
SUBTARGETS := $(foreach dir,$(SUBDIRS),$(dir)/$(dir).target)

.PHONY: $(SUBDIRS) dep clean touch

$(TARGET): $(OBJS) $(SUBDIRS)
	$(LD) $(LDFLAGS) $(LDFLAGS2) -o $@ $(OBJS) $(SUBTARGETS)

$(SUBDIRS):
	make -C $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGS2) -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) $(ASFLAGS2) -o $@ $< 

dep:
	@set -e
	@rm -f Depend
	@for i in $(SRCS); do $(CPP) $(CPPFLAGS) $$i >> Depend; done
	@for i in $(SUBDIRS); do make -C $$i dep; done

clean:
	@rm -f Depend *.o *.target
	@for i in $(SUBDIRS); do make -C $$i clean ; done
ifdef FULLLINK
	@rm -f Image
	@make -C boot clean
endif 

touch:
	@find . -exec touch {} \;
	@for i in $(SUBDIRS); do make -C $$i touch; done

ifeq (Depend,$(wildcard Depend))
include Depend
endif
