# the following include saves the command line flags into the MAKEDEF variable
!include "makeopts.mk"

ALLDIRS=module ldr plugins

.PHONY: env $(ALLDIRS) all

all: env allbin

env:
	@$(MAKE) -DDEBUG  -f makeenv_br.mak
	@$(MAKE) -DNDEBUG -f makeenv_br.mak
	@$(MAKE) -DDEBUG  -f makeenv_vc.mak
	@$(MAKE) -DNDEBUG -f makeenv_vc.mak

allbin: env $(ALLDIRS)

$(ALLDIRS):	env
	cd $@
	@$(MAKE)
	@cd ..

###
