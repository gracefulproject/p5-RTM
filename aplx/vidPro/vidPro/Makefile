#DIR := master worker
DIR = worker master

MK := make
LS := ls -l
RM := rm -f

all: $(DIR)
	@for d in $(DIR); do (cd $$d; $(MK)) || exit $$?; done
	@for d in $(DIR); do (cp $$d/*.aplx bin/); done

clean:
	@for d in $(DIR); do (cd $$d; $(MK) clean ) || exit $$?; done
	@$(RM) bin/mVidPro.aplx bin/wVidPro.aplx

