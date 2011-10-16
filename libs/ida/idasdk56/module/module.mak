!ifdef __VC__
_CFLAGS=$(__CFLAGS) -D__IDP__ -Gz
!else
_CFLAGS=$(__CFLAGS) -D__IDP__
!endif

BASE=0x13000000
__IDP__=1
!include ../../allmake.mak

!ifdef O1
OBJ1=$(F)$(O1)$(O)
!endif

!ifdef O2
OBJ2=$(F)$(O2)$(O)
!endif

!ifdef O3
OBJ3=$(F)$(O3)$(O)
!endif

!ifdef O4
OBJ4=$(F)$(O4)$(O)
!endif

!ifdef O5
OBJ5=$(F)$(O5)$(O)
!endif

!ifdef O6
OBJ6=$(F)$(O6)$(O)
!endif

!ifdef O7
OBJ7=$(F)$(O7)$(O)
!endif

!ifdef H1
HELPS=$(H1)$(HH)
!endif

OBJS=$(F)ins$(O) $(F)ana$(O) $(F)out$(O) $(F)reg$(O) $(F)emu$(O) \
     $(L)ident$(O) \
     $(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5) $(OBJ6) $(OBJ7) \
     $(ADDITIONAL_FILES)

!ifndef MAKE_HERE
BIN_PATH=$(R)procs\\
!endif
IDP_MODULE=$(BIN_PATH)$(PROC)$(IDP)

!ifndef __X64__
DEFFILE=..\idp.def
!else
DEFFILE=..\idp64.def
!endif

all:    objdir $(HELPS) $(IDP_MODULE) $(ADDITIONAL_GOALS)

$(IDP_MODULE): $(OBJS) $(IDALIB) $(DEFFILE)
!ifdef LINK_NOBOR
	$(LINKER) $(LINKOPTS) /STUB:..\stub /OUT:$@ $(OBJS) $(IDALIB) user32.lib
        -@$(RM) $(@R).exp
        -@$(RM) $(@R).lib
!else
        $(LINKER) @&&~
$(LINKOPTS) $(IDPSTUB) $(OBJS)
$@

$(IDALIB) $(IDPSLIB)
$(DEFFILE)
~
!ifndef __X64__
        $(PEUTIL) -d$(DEFFILE) $@
!endif
!endif
!ifdef DESCRIPTION
        $(RS)mkidp$(BS) $@ "$(DESCRIPTION)"
!endif

!include ../../objdir.mak

xml: $(C)$(PROC).xml
$(C)$(PROC).xml: $(PROC).xml
!ifndef MAKE_HERE
	$(CP) $? $@
!endif
