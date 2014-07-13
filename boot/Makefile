%.o:%.cc
	g++ -c -O -o $@ $<

%.o:%.S
	g++ -c -D__ASSEMBLY__ -o $@ $<

all: glue bootsect setup bootparam

glue: glue.o
	g++ -o $@ $^

bootsect: bootsect.o
	ld -e 0 -Ttext 0 -o $@ $@.o

setup: setup.o
	ld -e 0 -Ttext 0 -o $@ $@.o

bootparam: bootparam.o
	g++ -o $@ $^

SRCS:=glue.cc bootsect.S setup.S bootparam.cc
dep:
	@rm -f Depend
	@for s in $(SRCS); do cpp -MM $$s >>Depend; done

clean:
	@rm -f *.o glue bootsect setup collect collect.nm bootparam Depend

ifeq (Depend, $(wildcard Depend))
include Depend
endif
