#
#       Common part of make files for IDA for MS Windows.
#
#       All makesfiles are prepared to be used by Borland's MAKE.EXE

# Main IDA directory. PLEASE EDIT OR SET ENVVAR!
!ifndef IDA
IDA=Z:\idasrc\sdk\  		# with the trailing backslash!
!endif

!include $(IDA)makeopts.mk
!include $(IDA)defaults.mk

_MKFLG=-$(MAKEFLAGS)
MAKEDEFS=$(_MKFLG:--=-) $(MAKEDEFS)

!ifdef __X64__
__EA64__=1
__VC__=1                # only Visual Studio can compile 64-bit
!endif

!ifdef __CEARM__
__VC__=1
__ARM__=1
!endif

!ifdef __ARM__
PROCFLAG=-D__ARM__
TARGET_PROCESSOR=arm
!else
TARGET_PROCESSOR=pc
!endif

!ifndef BC5_COM
BC5_COM=$(BCB)
!endif

ULINK_BCF=+$(ULNK_CFG_DIR)ulink.bc
ULINK_64F=+$(ULNK_CFG_DIR)ulink.vx
ULINK_VCF=+$(ULNK_CFG_DIR)ulink.v9

!ifdef __X64__
ULNK_CFG=$(ULINK_64F)
!elseif defined(__VC__)
ULNK_CFG=$(ULINK_VCF)
!else
ULNK_CFG=$(ULINK_BCF)
!endif

ULNK_COMPAT=-O- -o- -Gh -Gh-
!ifndef __X64__
ULNK_COMPAT=$(ULNK_COMPAT) -P-
!endif

ULINK=$(ULNK_BASE) $(ULNK_CFG) $(ULINK_COMPAT_OPT)

_ULCCOPT=$(_LDFLAGS) $(LNDEBUG) $(_LNK_EXE_MAP) $(_LSHOW)

!ifdef __X64__
!if "$(PROCESSOR_ARCHITEW6432)" != "AMD64"
NO_EXECUTE_TESTS = 1
!endif
!endif

OPTLIBDIR=opt
!ifdef NDEBUG
CFGSUF=opt
OBJDIRSUF=opt
!else
!endif

#-----------
!ifdef __CEARM__
LINK_NOBOR = 1
!endif

!ifdef LINK_NOBOR
!undef LINK_ULINK
!endif

!ifdef __VC__
!ifndef LINK_ULINK
!undef LINK_NOBOR
LINK_NOBOR = 1
!endif
!endif

!ifndef LINK_NOBOR
!ifdef MAP
_LNK_EXE_MAP=-m
!else
_LNK_EXE_MAP=-x
!endif
!else               # LINK_NOBOR
!ifdef __VC__
!ifdef MAP
_LNK_EXE_MAP=/MAP
!endif
!endif
!endif

!ifndef __X64__
!ifndef __CEARM__
!ifndef VC32_BIN
VC32_BIN=\vc32
!endif
!ifdef __VC__
_VCBIN=$(VC32_BIN)
!endif
!endif
!endif

#------
!ifndef AROPT
AROPT=ru
!endif

!ifndef NOSHOW
AROPT=$(AROPT)v
!else
.silent
_LDSW=_q

!ifdef __VC__
_CSHOW=/nologo
!else
_CSHOW=-q
!endif

!ifndef LINK_NOBOR
_LSHOW=-q
!else
_LSHOW=/nologo
!endif

_LBSHOW=_f/nologo

!endif   # NOSHOW

#-----------

!ifndef IDALIBS
IDALIBS=$(IDA)
!endif

######################### set TV variables
!ifndef TVSRC
TVSRC=$(IDA)ui\txt\tv\  # TurboVision subdirectory
!endif

!ifndef IDA_TVL
IDA_TVL=$(TVSRC)
!endif

!ifdef _CFLAGS_TV       # set tv path(s) for ui/txt after include defaults.mk
_CFLAGS=-I$(TVSRC)include $(_CFLAGS_TV)
!endif

############################################################################
.PHONY: all All goal Goal total Total objdir test runtest $(ADDITIONAL_GOALS)

######################### set debug switches
!ifndef NDEBUG
LNDEBUG=-v
CCDEBUG=-v
CCOPT=-O-
CCDEBUG=-v -D_DEBUG
!else
CCOPT=-O2
!endif

!ifdef __X64__
__EA64__=1
_SWITCH64=-D__X64__
X64SUFF=x
!endif

!ifdef __EA64__
SUFF64=64
ADRSIZE=64
SWITCH64=$(_SWITCH64) -D__EA64__
!else
ADRSIZE=32
!endif

# include,help and other directories are common for all platforms and compilers:
R =$(IDA)bin$(_VCBIN)$(ALTBIN)\         # main result directory
!if !$d(_VCBIN) && $d(VC32_BIN)
R_VC32=$(IDA)bin\$(VC32_BIN)$(ALTBIN)\  # used for copy compiled Clp
!endif
I =$(IDA)include\       # include directory
HO=$(R)                 # help file is stored in the bin directory
HI=$(RS)                # _ida.hlp placed in main tool directory
C =$(R)cfg\             # cfg files stored path
RI=$(R)idc\             # idc files stored path
HS=.hls                 #       help source
HH=.hhp                 #       help headers
SYSNAME=win

#############################################################################
!if $d(__X64__)                                         # Visual Studio 8 for AMD64
CC      =$(MSVCDIR)bin\x86_amd64\cl.exe                 # C++ compiler
!ifndef NDEBUG
CCDEBUG=/Zi
CCOPT=/Od
!ifndef LINK_ULINK
LNDEBUG=/DEBUG
!endif
PDBOUT=/Fd$(F)
!else
CCOPT=/O2
!endif
CFLAGS  =@$(IDA)wx64vs.cfg$(CFGSUF) $(SWITCH64) $(CCDEBUG) $(CCOPT) $(_CFLAGS) $(_CSHOW) # default compiler flags
OUTSWC  =/Fe                                            # outfile name switch for one-line linker
!ifndef LINK_ULINK
OUTSW   =/Fe                                            # outfile name switch for one-line linker
OBJSW   =/Fo                                            # object file name switch
BASESW  =/BASE
OUTDLL  =/LD
LNOUTDLL=/DLL
!else
OUTSW   =-ZO
OUJSW   =-j
BASESW  =-b
LNOUTDLL=-Tpd+                                          # output is DLL(64) switch
OUTDLL  =-Tpd+
!endif
!ifdef BASE
LDFLAGS =$(BASESW):$(BASE) $(_LDFLAGS)
!else
LDFLAGS =$(_LDFLAGS)
!endif
_LIBRTL=$(MSVCDIR)lib\amd64
!ifndef VC_WITHOUT_PSDK
_LIBSDK=$(MSVCDIR)PlatformSDK\lib\amd64
NOBOR_PATH=/LIBPATH:$(_LIBSDK)
!elif defined(MSSDK)
_LIBSDK=$(MSSDK)lib\x64
NOBOR_PATH=/LIBPATH:$(_LIBSDK)
!endif
!ifdef LINK_NOBOR
LINKOPTS_EXE=/LIBPATH:$(_LIBRTL) $(NOBOR_PATH) $(_LNK_EXE_MAP)
!else
LINKOPTS_EXE=/L$(_LIBRTL);$(_LIBSDK) $(_LNK_EXE_MAP) $(LNDEBUG)
!endif
LINKOPTS=$(LNOUTDLL) $(LINKOPTS_EXE) $(_LSHOW)
!ifndef LINK_ULINK
_LINKER =$(MSVCDIR)bin\x86_amd64\link.exe               # indirect file linker
_LDAD=_a"/link $(LINKOPTS_EXE) $(CCL_LNK_OPT)"
!else
_LINKER =$(ULINK)                                       # indirect file linker
!endif
LINKER  = $(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)    # default link command
!ifndef LINK_ULINK
CCL     =$(RS)ld $(_LDSW) _v $(_LDAD) _l$(CC) $(CFLAGS) $(_LDFLAGS)  # one-line linker
!else
CCL     =$(RS)ld $(_LDSW) _b _l$(ULINK) $(_ULCCOPT)
!endif
LINKSYS_EXE=                                            # target link system for executable
LINKSYS =                                               # target link system
C_STARTUP=                                              # indirect linker: C startup file
C_LIB   =kernel32.lib                                   # import library
OVRON   =                                               # 16bit: following files overlayed
OVROFF  =                                               # 16bit: following files not overlayed
LIBBASE =$(IDALIBS)lib\vc.x64
LIBDIR  =$(LIBBASE)$(LIBDIRSUF)                         # libraries directory
TL      =$(IDA_TVL)lib\vc.x64$(LIBDIRSUF)\              # TVision libraries directory
OBJDIR  =obj\vc.x64$(OBJDIRSUF)                         # object files directory
B       =x64.exe                                        # exe file extension
BS      =.exe                                           # host utility extension
MAP     =.mpv                                           # map file extension
IDP     =.x64                                           # IDP extension
DLLEXT  =64x.wll
IDP     =64.x64                                         # IDP extension
LDR     =64.x64                                         # LDR extension
PLUGIN  =.x64                                           # PLUGIN extension
O       =.obj                                           # object file extension
A       =.lib                                           # library file extension
!if  !$d(NORTL)
IDPSTUB =                                               # STUB file for IDPs
LDRSTUB =                                               # STUB file for LDRs
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
!else
IDPSTUB =$(LIBDIR)\modstart                             # STUB file for IDPs
LDRSTUB =$(LIBDIR)\modstart                             # STUB file for LDRs
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
!endif
AR      =$(RS)ar$(BS) _e.at _v _l$(MSVCDIR)bin\x86_amd64\lib.exe $(_LBSHOW) $(AROPT) # librarian
APISW   =-swin -i$(RS)ida.imp -n
# force C mode for .c files
!if $d(DONT_FORCE_CPP)
FORCEC=/TC
!endif
#############################################################################
!elif $d(__CEARM__)                                     # Visual C++ v4.0 for ARM 4.20
CC      ="$(MSVCARMDIR)bin\x86_arm\cl.exe"                 # C++ compiler
CCOPT=/O2
CCDEBUG=
LNDEBUG=
CFLAGS  =@$(IDA)cearm.cfg$(CFGSUF) $(SWITCH64) $(PROCFLAG) $(CCDEBUG) $(CCOPT) $(_CFLAGS) $(_CSHOW) # default compiler flags
##CFLAG_SUFFIX = /link /subsystem:windowsce
OUTSWC  =/Fe                                            # outfile name switch for one-line linker
OUTSW   =/Fe                                            # outfile name switch for one-line linker
OBJSW   =/Fo                                            # object file name switch
!ifdef BASE
LDFLAGS =/BASE:$(BASE) $(_LDFLAGS)
!else
LDFLAGS =$(_LDFLAGS)
!endif
OUTDLL  =-LD
LINKOPTS_EXE=/LIBPATH:"$(MSVCARMDIR)lib\armv4" /LIBPATH:"$(ARMSDK)lib\armv4"
LINKOPTS=$(LINKOPTS_EXE) $(_LSHOW)
_LINKER =$(MSVCARMDIR)bin\x86_arm\link.exe                   # indirect file linker
LINKER  =$(_LINKER) $(LDFLAGS) $(LNDEBUG)               # default link command
_LDAD=_a"/link /subsystem:windowsce,4.20 /machine:arm /armpadcode $(CCL_LNK_OPT) /LIBPATH:\"$(MSVCARMDIR)lib\armv4\" /LIBPATH:\"$(ARMSDK)lib\armv4\""
CCL     =$(RS)ld $(_LDSW) _c $(_LDAD) _l$(CC) $(CFLAGS) $(_LDFLAGS) # one-line linker
C_LIB   =corelibc.lib coredll.lib                       # import library
LIBDIR  =$(IDA)lib\armc.e32$(LIBDIRSUF)                 # libraries directory
OBJDIR  =obj\armc.e32$(OBJDIRSUF)                       # object files directory
B       =_arm.exe                                       # exe file extension
BS      =.exe                                           # host utility extension
MAP     =.mparm                                         # map file extension
IDP     =.cearm32                                       # IDP extension
DLLEXT  =.dll
IDP     =.cearm32                                       # IDP extension
LDR     =.cearm32                                       # LDR extension
PLUGIN  =.cearm32                                       # PLUGIN extension
O       =.obj                                           # object file extension
A       =.lib                                           # library file extension
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
_LIBR   =$(MSVCARMDIR)bin\x86_arm\lib.exe
AR      =$(RS)ar$(BS) _e.at _v "_l$(_LIBR)" $(_LBSHOW) $(AROPT) # librarian
APISW   =-swin -i$(RS)ida.imp -n
# force C mode for .c files
!if $d(DONT_FORCE_CPP)
FORCEC=/TC
!endif
_ARMASM ="$(MSVCARMDIR)bin\x86_arm\armasm.exe"
#############################################################################
!elif $d(__VC__)                                        # Visual Studio 2008 for x86
CC      =$(MSVCDIR)bin\cl.exe                           # C++ compiler
!ifndef NDEBUG
CCDEBUG=/Zi
CCOPT=/Od
!ifndef LINK_ULINK
LNDEBUG=/DEBUG
!endif
PDBOUT=/Fd$(F)
!else
CCOPT=/O2
!endif
CFLAGS  =@$(IDA)w$(ADRSIZE)vs.cfg$(CFGSUF) $(SWITCH64) $(CCDEBUG) $(CCOPT) $(_CFLAGS) $(_CSHOW) # default compiler flags
OUTSWC  =/Fe                                            # outfile name switch for one-line linker
!ifndef LINK_ULINK
OUTSW   =/Fe                                            # outfile name switch for one-line linker
OBJSW   =/Fo                                            # object file name switch
BASESW  =/BASE
OUTDLL  =/LD
LNOUTDLL=/DLL
!else
OUTSW   =-ZO
OUJSW   =-j
BASESW  =-b
LNOUTDLL=-Tpd                                           # output is DLL(32) switch
OUTDLL  =-Tpd
!endif
!ifdef BASE
LDFLAGS =$(BASESW):$(BASE) $(_LDFLAGS)
!else
LDFLAGS =$(_LDFLAGS)
!endif
_LIBRTL=$(MSVCDIR)lib
!ifndef VC_WITHOUT_PSDK
_LIBSDK=$(MSVCDIR)PlatformSDK\lib
NOBOR_PATH=/LIBPATH:$(_LIBSDK)
!elif defined(MSSDK)
_LIBSDK=$(MSSDK)lib
NOBOR_PATH=/LIBPATH:$(_LIBSDK)
!endif
!ifdef LINK_NOBOR
LINKOPTS_EXE=/LIBPATH:$(_LIBRTL) $(NOBOR_PATH) $(_LNK_EXE_MAP)
!else
LINKOPTS_EXE=/L$(_LIBRTL);$(_LIBSDK) $(_LNK_EXE_MAP) $(LNDEBUG)
!endif
LINKOPTS=$(LNOUTDLL) $(LINKOPTS_EXE) $(_LSHOW) $(LNDEBUG)
!ifndef LINK_ULINK
_LINKER =$(MSVCDIR)bin\link.exe                         # indirect file linker
_LDAD=_a"/link $(LINKOPTS_EXE)"
!else
_LINKER =$(ULINK)                                       # indirect file linker
!endif
LINKER  = $(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)    # default link command
!ifndef LINK_ULINK
CCL     =$(RS)ld $(_LDSW) _v $(_LDAD) _l$(CC) $(CFLAGS) $(_LDFLAGS)  # one-line linker
!else
CCL     =$(RS)ld $(_LDSW) _b _l$(ULINK) $(_ULCCOPT)
!endif
LINKSYS_EXE=                                            # target link system for executable
LINKSYS =                                               # target link system
C_STARTUP=                                              # indirect linker: C startup file
C_LIB   =kernel32.lib                                   # import library
OVRON   =                                               # 16bit: following files overlayed
OVROFF  =                                               # 16bit: following files not overlayed
LIBBASE =$(IDALIBS)lib\vc.w$(ADRSIZE)
LIBDIR  =$(LIBBASE)$(LIBDIRSUF)                         # libraries directory
TL      =$(IDA_TVL)lib\vc.w$(ADRSIZE)$(LIBDIRSUF)\      # TVision libraries directory
OBJDIR  =obj\vc.w$(ADRSIZE)$(OBJDIRSUF)                 # object files directory
B       =v32.exe                                        # exe file extension
BS      =.exe                                           # host utility extension
MAP     =.mpv                                           # map file extension
!ifdef __EA64__
DLLEXT  =64.wll
IDP     =64.w$(ADRSIZE)                                 # IDP extension
LDR     =64.l$(ADRSIZE)                                 # LDR extension
PLUGIN  =.p$(ADRSIZE)                                   # PLUGIN extension
!else
DLLEXT  =.wll
IDP     =.w$(ADRSIZE)
LDR     =.ldw
PLUGIN  =.plw
!endif
O       =.obj                                           # object file extension
A       =.lib                                           # library file extension
!if  !$d(NORTL)
IDPSTUB =                                               # STUB file for IDPs
LDRSTUB =                                               # STUB file for LDRs
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
!else
IDPSTUB =$(LIBDIR)\modstart                             # STUB file for IDPs
LDRSTUB =$(LIBDIR)\modstart                             # STUB file for LDRs
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
!endif
AR      =$(RS)ar$(BS) _e.at _v _l$(MSVCDIR)bin\lib.exe $(_LBSHOW) $(AROPT) # librarian
APISW   =-swin -i$(RS)ida.imp -n
# force C mode for .c files
!if $d(DONT_FORCE_CPP)
FORCEC=/TC
!endif
#############################################################################
!else                                                   # Borland C++ for NT (WIN32)
IMPLIB  =$(BCB)\bin\implib.exe                          # implib executable name
ASM     =$(BC5_COM)\bin\tasm32.exe                      # assembler
CC      =$(BCB)\bin\bcc32.exe                           # C++ compiler
!ifdef __IDP__
CC_ALIGN= -ps                                           # Standard calling convention
!else
CC_ALIGN= -pr                                           # Register calling convention
!endif
!ifdef __PRECOMPILE__
CC_PRECOMPILE= -H
!endif
CFLAGS  =+$(IDA)w$(ADRSIZE)bor.cfg$(CFGSUF) $(SWITCH64) $(CC_PRECOMPILE) $(CC_ALIGN) $(CCDEBUG) $(CCOPT) $(_CFLAGS) $(_CSHOW) # default compiler flags
AFLAGS  =/D__FLAT__ /t/ml/m5$(_AFLAGS)                  # default assembler flags
OUTSWC  =-n -e                                          # outfile name switch for one-line linker
!ifndef LINK_ULINK
OUTSW   =-n -e                                          # outfile name switch for one-line linker
!else
OUTSW   =-ZO                                            # outfile name switch for one-line linker
!endif
OBJSW   =-n. -o                                         # object file switch (only current directory is supported)
OUTDLL  =/Tpd                                           # output is DLL switch
!ifdef BASE
NT_BSW  =-b=$(BASE)
!endif
LDFLAGS =$(NT_BSW) $(_LDFLAGS)
!ifdef LINK_ULINK
_LINKER =$(ULINK)                                       # indirect file linker
!else
_LINKER =$(BCB)\bin\ilink32.exe -Gn                     # indirect file linker
!endif
LINKER  = $(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)    # default link command
LINKSYS_EXE=                                            # target link system for executable
LINKSYS =                                               # target link system
C_STARTUP=c0x32                                         # indirect linker: C startup file
C_IMP   =import32.lib                                   # import library
C_LIB   =$(C_IMP) cw32mt.lib                            # indirect linker: default C library
!ifndef LINK_ULINK
CCL     =$(RS)ld $(_LDSW) _b _l$(CC) $(CFLAGS) $(_LDFLAGS)       # one-line linker
!else
CCL     =$(RS)ld $(_LDSW) _b _a"$(C_LIB)" _l$(ULINK) $(_ULCCOPT) $(C_STARTUP)
!endif
OVRON   =                                               # 16bit: following files overlayed
OVROFF  =                                               # 16bit: following files not overlayed
LIBBASE =$(IDALIBS)lib\bor.w$(ADRSIZE)
LIBDIR  =$(LIBBASE)$(LIBDIRSUF)                         # libraries directory
OBJDIR  =obj\bor.w$(ADRSIZE)$(OBJDIRSUF)                # object files directory
!ifdef __EA64__
B       =64.exe                                         # exe file extension
!else
B       =.exe                                           # exe file extension
!endif
BS      =.exe                                           # host utility extension
MAP     =.mpb                                           # map file extension
IDP     =.w$(ADRSIZE)                                   # IDP extension
!ifdef __EA64__
DLLEXT  =64.wll
IDP     =64.w$(ADRSIZE)                                 # IDP extension
LDR     =64.l$(ADRSIZE)                                 # LDR extension
PLUGIN  =.p$(ADRSIZE)                                   # PLUGIN extension
!else
DLLEXT  =.wll
LDR     =.ldw
PLUGIN  =.plw
!endif
ORDINALS= #-o                                           # import functions by ordinals
# -c case sensitive
# -C clear state before linking
# -s detailed map of segments
# -m detailed map of publics
# -r verbose
LINKOPTS_EXE= $(_LNK_EXE_MAP) -c -C $(ORDINALS) $(LNDEBUG) -L$(BCB)\lib
LINKOPTS=$(OUTDLL) $(LINKOPTS_EXE) $(_LSHOW)
O       =.obj                                           # object file extension
A       =.lib                                           # library file extension
!if  !$d(NORTL)
IDPSTUB =$(BCB)\lib\c0d32                               # STUB file for IDPs
LDRSTUB =$(BCB)\lib\c0d32                               # STUB file for LDRs
IDPSLIB =$(C_LIB)                                       # system libraries for IDPs
!else
IDPSTUB =$(LIBDIR)\modstart                             # STUB file for IDPs
LDRSTUB =$(LIBDIR)\modstart                             # STUB file for LDRs
IDPSLIB =$(C_IMP)                                       # system libraries for IDPs
!endif
AR      =$(RS)ar$(BS) _a _e.at "_l$(TLIB)" _f/C/E/P128 $(AROPT) # librarian
APISW   =-swin -i$(RS)ida.imp -n
# force C mode for .c files
!if $d(DONT_FORCE_CPP)
FORCEC=-P-
!endif
!endif
#############################################################################

!if 0           # this is for makedep
F=
!else
F=$(OBJDIR)\                        # object files dir with backslash
L=$(LIBDIR)\                        # library files dir with backslash
!endif

HC=$(RS)ihc$(BS)                                # Help Compiler

IDALIB=$(L)ida$(A)
DUMB=$(L)dumb$(O)
HLIB=$(HI)_ida.hlp

########################################################################
MAKEDEFS=$(MAKEDEFS) -U__MSDOS__ -D__NT__

!ifdef __EA64__
!ifdef __X64__
MAKEDEFS=$(MAKEDEFS) -D__X64__
!else
MAKEDEFS=$(MAKEDEFS) -D__EA64__
!endif
!endif
!ifdef __VC__
MAKEDEFS=$(MAKEDEFS) -D__VC__
!endif

### for 'if exist DIRECTORY'
!IF "$(OS)" == "Windows_NT"
CHKDIR=
!ELSE
CHKDIR=/nul
!ENDIF

########################################################################
!ifndef CONLY
CONLY=-c
!endif

.cpp$(O):
        $(CC) $(CFLAGS) $(CONLY) $(PDBOUT) {$< }
.c$(O):
        $(CC) $(CFLAGS) $(CONLY) $(FORCEC) $(PDBOUT) {$< }
.asm$(O):
        $(ASM) $(AFLAGS) $*,$(F)$*

.hls.hhp:
        $(HC) -t $(HLIB) -i$@ $?
########################################################################
