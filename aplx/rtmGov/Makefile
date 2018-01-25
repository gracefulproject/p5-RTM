MKFILES = Makefile.arm Makefile.thumb

RM := \rm -rf
LS := \ls -l
CP := \cp

all: $(MKFILES)
	@for m in $(MKFILES); do ("$(MAKE)" -f $$m) || exit $$?; done

clean: $(MKFILES)
	@for m in $(MKFILES); do ("$(MAKE)" clean -f $$m) || exit $$?; done
	@$(RM) ../bin/stdGov.aplx


