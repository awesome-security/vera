include allmake.mak
ifndef __UNIX__
  ifdef __VC__
    CFGFILE := makeenv_vc.mak
  else
    CFGFILE := makeenv_br.mak
  endif
  ENV := env
endif

ALLDIRS=module ldr plugins

.PHONY: env $(ALLDIRS) all

all: $(ENV) allbin

env:
	@$(MAKE) -f $(CFGFILE)

allbin: $(ENV) $(ALLDIRS)

$(ALLDIRS): $(ENV)
	@cd $@; $(MAKE)
###
