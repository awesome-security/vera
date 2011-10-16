# the following include saves the command line flags into the MAKEDEF variable
!include "makeopts.mk"

!ifdef NO_KERNEL
ALLDIRS1=ui
ALLDIRS2=module ldr plugins
INCDIRS=
!else
INCDIRS=incdirs
!ifdef __CEARM__
ALLDIRS1=pro plugins/debugger
!else
ALLDIRS1=pro hc mkdep regex clp btree netnode vm va compress pattern demangle \
         parse register
!ifndef KERNEL
ALLDIRS2=efd base info ida ui module ldr plugins
!else
ALLDIRS2=base info ida
!endif
HELPDIRS=ui/txt
!endif
!endif

ALLDIRS=etc $(INCDIRS) $(ALLDIRS1) $(HELPDIRS) $(ALLDIRS2)

.PHONY: env $(ALLDIRS) all incdirs

all: env allbin

env:
	@$(MAKE) -DDEBUG  -f makeenv_br.mak
	@$(MAKE) -DNDEBUG -f makeenv_br.mak
	@$(MAKE) -DDEBUG  -f makeenv_vc.mak
	@$(MAKE) -DNDEBUG -f makeenv_vc.mak

allbin: env $(ALLDIRS)

etc:	env
        @cd etc
	@$(MAKE)
        @cd ..

incdirs:	env
	@cd base
        @$(MAKE) includes
        @cd ..

!ifdef HELPDIRS
$(HELPDIRS):
	@cd $@
	@$(MAKE) help
	@cd ..\..
!endif

$(ALLDIRS1) $(ALLDIRS2) efd:	env
	cd $@
	@$(MAKE)
	@cd ..

###
