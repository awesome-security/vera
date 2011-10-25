#
#       Common part of make files for IDA under MS Windows.
#

# find directory of allmake.mak:
IDA:=$(dir $(lastword $(MAKEFILE_LIST)))

# define the version number we are building
IDAVER_MAJOR:=6
IDAVER_MINOR:=2
# 620
IDAVERDECIMAL:=$(IDAVER_MAJOR)$(IDAVER_MINOR)0
# 6.2
IDAVERDOTTED:=$(IDAVER_MAJOR).$(IDAVER_MINOR)

# unix: redirect to allmake.unx
ifneq ($(or $(__MAC__),$(__LINUX__)),)
include $(IDA)allmake.unx
else
# the rest of the file is for windows

# define: convert unix path to dos path by replacing slashes by backslashes
dospath=$(subst /,\\,$(1))
# define: convert dos path to unix path by replacing backslashes by slashes
unixpath=$(subst \,/,$(1))
# define: return absolute path given a relative path
qabspath=$(subst /cygdrive/z,z:,$(abspath $(1)))/
# define: logical negation
not = $(if $(1),,1)
# define: comma to use in function calls as a literal
comma=,

include $(IDA)defaults.mk

# if none of these are defined, default to __NT__
ifeq ($(or $(__ANDROID__),$(__ARMLINUX__),$(__LINUX__),$(__MAC__)),)
  __NT__=1
endif

NO_ULINK=1
ifndef NO_ULINK
  LINK_ULINK := 1
endif

ifndef NDEBUG
  DEBUG := 1
endif

ifndef VC_USE_CPUS
  VC_USE_CPUS := 8
endif

# force visual studio for WinCE and x64
ifneq ($(or $(__CEARM__),$(__X64__)),)
  __VC__=1
endif

# since we can not debug android/cearm servers, there is no point in building
# them with debug info. always build the optimized version.
ifneq ($(or $(__ANDROID__),$(__CEARM__),$(__ARMLINUX__)),)
  NDEBUG=1
  __ARM__=1       # both of them run on arm processor
endif

ifdef __ARM__
  PROCFLAG=-D__ARM__
  TARGET_PROCESSOR_MODULE=arm
else
  TARGET_PROCESSOR_MODULE=pc
endif

BC5_COM?=$(BCB)

ULINK_BCF=+$(ULNK_CFG_DIR)ulink.bc
ULINK_64F=+$(ULNK_CFG_DIR)ulink.vx
ULINK_VCF=+$(ULNK_CFG_DIR)ulink.v9

ifdef __X64__
  ULNK_CFG=$(ULINK_64F)
else
  ifdef __VC__
    ULNK_CFG=$(ULINK_VCF)
  else
    ULNK_CFG=$(ULINK_BCF)
  endif
endif

ULNK_COMPAT=-O- -o- -Gh -Gh-
ifndef __X64__
  ULNK_COMPAT=$(ULNK_COMPAT) -P-
endif

ULINK=$(ULNK_BASE) $(ULNK_CFG) $(ULINK_COMPAT_OPT)

_ULCCOPT=$(_LDFLAGS) $(LNDEBUG) $(_LNK_EXE_MAP) $(_LSHOW)

ifdef __X64__
  ifneq ($(PROCESSOR_ARCHITEW6432),AMD64)
    NO_EXECUTE_TESTS = 1
  endif
endif

ifdef NDEBUG
  OPTSUF=_opt
endif

#-----------
ifdef __VC__
  ifndef LINK_ULINK
    # Use normal one line (non-borland) linker
    LINK_NOBOR:=1
  endif
endif

ifdef LINK_NOBOR
  ifdef __VC__
    ifdef MAP
      _LNK_EXE_MAP=/MAP
    endif
  endif
else
  ifdef MAP
    _LNK_EXE_MAP=-m
  else
    _LNK_EXE_MAP=-x
  endif
endif

#------
AROPT?=ru
ifndef NOSHOW
  AROPT:=$(AROPT)v
else
  .SILENT:
  _LDSW=_q

  ifdef __VC__
    _CSHOW=/nologo
  else
    _CSHOW=-q
  endif

  ifdef LINK_NOBOR
    _LSHOW=/nologo
  else
    _LSHOW=-q
  endif

  _LBSHOW=_f/nologo
endif   # NOSHOW

ifdef SUPPRESS_WARNINGS
  ifdef __ANDROID__
    NOWARNS=-w
  else
    ifdef __VC__
      NOWARNS=-w -wd4702 -wd4738
    else # BCC
      NOWARNS=-w-
    endif
  endif
endif

LDEXE=$(RS)ld$(BS) $(_LDSW)

######################### set TV variables
ifndef TVSRC
  TVSRC=$(IDA)ui/txt/tv/
endif

ifdef _CFLAGS_TV       # set tv path(s) for ui/txt after include defaults.mk
  _CFLAGS=-I$(TVSRC)include $(_CFLAGS_TV)
endif

############################################################################
.PHONY: all All goal Goal total Total objdir test runtest $(ADDITIONAL_GOALS)

######################### set linker debug switch
ifneq ($(and $(__VC__),$(call not,$(LINK_ULINK))),)
  ifdef NDEBUG
    LNDEBUG=/DEBUG /INCREMENTAL:NO /OPT:ICF /OPT:REF /PDBALTPATH:%_PDB%
  else
    LNDEBUG=/DEBUG 
  endif
else
  ifndef NDEBUG
    LNDEBUG=-v
  endif
endif

#########################
# visual studio: set c runtime to use
# default is dynamic runtime
# if USE_STATIC_RUNTIME is set, use static runtime
# in that case, use VS8 for better compatibility
ifneq ($(and $(__VC__),$(NDEBUG),$(call not,$(RUNTIME_LIBSW))),)
  ifdef USE_STATIC_RUNTIME
    RUNTIME_LIBSW=/MT
    STATSUF=_s
    MSVCDIR=$(VSPATH8)VC/
  else
    RUNTIME_LIBSW=/MD
  endif
else
  ifdef USE_STATIC_RUNTIME
    RUNTIME_LIBSW=/MTd
    STATSUF=_s
    MSVCDIR=$(VSPATH8)VC/
  else
    RUNTIME_LIBSW=/MDd
  endif
endif

ifdef __X64__
  __EA64__=1
  SWITCH64=-D__X64__
  X64SUFF=x
endif

ifdef __EA64__
  SUFF64=64
  ADRSIZE=64
  SWITCH64+=-D__EA64__
else
  ADRSIZE=32
endif

# include,help and other directories are common for all platforms and compilers:
I =$(IDA)include/
# help file is stored in the bin directory
HO=$(R)
# _ida.hlp placed in main tool directory
HI=$(RS)
C =$(R)cfg/
RI=$(R)idc/
# help source
HS=.hls
# help headers
HH=.hhp
SYSNAME=win

CCX=$(CC)

#############################################################################
# To compile debugger server for Android, Android NDK should be installed
# Environment variable ANDROID_NDK must point to it (default c:/android-ndk-r4b/)
# To cross-compile for ARM Linux/uCLinux, define SOURCERY root directory
# (default C:/CodeSourcery/Sourcery G++ Lite)
ifneq ($(or $(__ANDROID__),$(__ARMLINUX__)),)
  ifneq ($(or $(__NT__),$(__VC__),$(__EA64__),$(__X64__)),)
    $(error "Please undefine __NT__, __VC__, __EA64__, __X64__ to compile for Android/ARM Linux")
  endif
  ifdef NDEBUG
    CCOPT=-O3 -ffunction-sections -fdata-sections
    OUTDLLOPTS=-Wl,-S,-x$(DEAD_STRIP)
  else
    CCOPT=-g
    OUTDLLOPTS=-Wl,--strip-debug,--discard-all
  endif
  BUILD_ONLY_SERVER=1
  COMPILER_NAME=gcc
  TARGET_PROCESSOR_NAME=arm
  TARGET_PROCESSOR_MODULE=arm
  ifdef __ANDROID__
    SYSNAME=android
    TARGET_PLATFORM=$(SYSNAME)-8
    CCDIR=$(ANDROID_NDK)build/prebuilt/windows/arm-eabi-4.4.0/bin
    CCPART=arm-eabi
    SYSROOT =$(ANDROID_NDK)build/platforms/$(TARGET_PLATFORM)/arch-arm
  else
    ifdef __UCLINUX__
      SYSNAME=uclinux
      CCPART=arm-uclinuxeabi
      __EXTRADEF=-D__UCLINUX__ -fno-exceptions -Wno-psabi
      __EXTRACPP=-fno-rtti
    else
      SYSNAME=linux
      CCPART=arm-none-linux-gnueabi
      __EXTRADEF=-Wno-psabi -fexceptions
    endif
    CCDIR=$(SOURCERY)/bin
    SYSROOT =$(SOURCERY)/$(CCPART)/libc
  endif
  CC      =$(CCDIR)/$(CCPART)-gcc.exe
  CCX     =$(CCDIR)/$(CCPART)-g++.exe
  SYSINC  =$(SYSROOT)/usr/include
  SYSLIB  =$(SYSROOT)/usr/lib
  ifdef __ANDROID__
    #BUILD_STATIC=1
    ifdef BUILD_STATIC
      CRTBEGIN=$(SYSROOT)/usr/lib/crtbegin_static.o
      SYS     =$(PROCFLAG) -mandroid -static
      LDSTATIC=-Bstatic
    else
      CRTBEGIN=$(SYSROOT)/usr/lib/crtbegin_dynamic.o
      SYS     =$(PROCFLAG) -mandroid --shared
      LDSTATIC=-Bdynamic
    endif
    CRTEND=$(SYSROOT)/usr/lib/crtend_android.o
    __EXTRADEF=-D__ANDROID__ -fno-exceptions -Wno-psabi
    __EXTRACPP=-fno-rtti
  endif
  STLDEFS=-D_M_ARM                      \
  	-D__linux__                     \
  	-D_STLP_HAS_NO_NEW_C_HEADERS    \
  	-D_STLP_NO_BAD_ALLOC            \
  	-D_STLP_NO_EXCEPTION_HEADER     \
  	-D_STLP_USE_NO_IOSTREAMS        \
  	-D_STLP_USE_MALLOC              \
  	-D_STLP_UINT32_T="unsigned long"

  CFLAGS=$(SYS) $(SWITCH64) $(CCOPT) -I$(I) -I$(STLDIR) -I$(SYSINC) \
  	-D__ARM__                                                 \
  	-D__LINUX__                                               \
  	$(__EXTRADEF)                                             \
  	-D_FORTIFY_SOURCE=0                                       \
  	-DNO_OBSOLETE_FUNCS                                       \
  	-DUSE_DANGEROUS_FUNCTIONS                                 \
  	$(STLDEFS)                                                \
  	-pipe -fno-strict-aliasing $(_CFLAGS)
  CPPFLAGS=-fvisibility=hidden -fvisibility-inlines-hidden $(_EXTRACPP) $(CFLAGS)
  OUTSW=-o #with space
  OBJSW=-o #with space
  STDLIBS =-lrt -lpthread
  ifdef __ANDROID__
    LDFLAGS =-nostdlib $(LDSTATIC) -Wl,-dynamic-linker,/system/bin/linker -Wl,-z,nocopyreloc $(_LDFLAGS)
    CCL     =$(CCX) $(LDFLAGS)
  else
    LDFLAGS =-Wl,-z $(_LDFLAGS)
    CCL     =$(CCX) $(LDFLAGS) $(STDLIBS)
  endif
  OUTDLL  =$(SYS) -Wl,--gc-sections -Wl,--no-undefined $(OUTDLLOPTS)
  LINK_NOBOR=1
  B       =
  BS      =.exe
  DLLEXT  =.so
  O       =.o
  A       =.a
  AR      =$(RS)ar$(BS) _e.at _g _l$(CCDIR)/$(CCPART)-ar.exe $(AROPT)
#############################################################################
else ifdef __LINT__                                       # PC-Lint
  COMPILER_NAME=lint
  TARGET_PROCESSOR_NAME=x86
  CC      =$(PYTHON) $(RS)pclint.py
  CFLAGS  =$(_CFLAGS) $(LINTFLAGS)
  OUTSW   =--outdir
  OBJSW   =--outdir
  LINK_NOBOR=1
  LINKER  =$(CC) --link
  CCL     =$(CC)
  AR      =$(CC) --lib
  O       =.lint
  B       =.lintexe
  A       =.lib
  R32     =$(RS)/x86_win_vc/
  B32     =$(BS)
  BS      =.exe
#############################################################################
else ifdef __X64__                                        # Visual Studio 2010 for AMD64
  COMPILER_NAME=vc
  TARGET_PROCESSOR_NAME=x64
  CC      =$(MSVCDIR)bin/x86_amd64/cl.exe
  CFLAGS  =@$(IDA)$(CFGNAME) $(RUNTIME_LIBSW) $(SWITCH64) $(NOWARNS) $(_CFLAGS) $(_CSHOW)
  ifndef LINK_ULINK
    OUTSW   =/Fe
    OBJSW   =/Fo
    BASESW  =/BASE
    OUTDLL  =/LD
    LNOUTDLL=/DLL
  else
    OUTSW   =-ZO
    OUJSW   =-j
    BASESW  =-b
    LNOUTDLL=-Tpd+
    OUTDLL  =-Tpd+
  endif
  ifdef BASE
    LDFLAGS =$(BASESW):$(BASE) $(_LDFLAGS)
  else
    LDFLAGS =$(_LDFLAGS)
  endif
  _LIBRTL=$(MSVCDIR)lib/amd64
  ifdef LINK_NOBOR
    LINKOPTS_EXE=/LIBPATH:$(_LIBRTL) /LIBPATH:$(MSSDK)lib/x64 $(_LNK_EXE_MAP) $(LNDEBUG)
  else
    LINKOPTS_EXE=/L$(_LIBRTL);$(MSSDK)lib/x64 $(_LNK_EXE_MAP) $(LNDEBUG)
  endif
  LINKOPTS=$(LNOUTDLL) $(LINKOPTS_EXE) $(_LSHOW)
  ifdef LINK_ULINK
    _LINKER =$(ULINK)
    CCL     =$(LDEXE) _b _l$(ULINK) $(_ULCCOPT)
  else
    _LINKER =$(LDEXE) _v _l$(MSVCDIR)bin/x86_amd64/link.exe
    CCL     =$(LDEXE) _v _l$(CC) _a"/link $(LINKOPTS_EXE) $(CCL_LNK_OPT)" $(CFLAGS) $(_LDFLAGS)
  endif
  LINKER=$(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)
  C_STARTUP=
  C_LIB   =kernel32.lib
  B       =x64.exe
  BS      =.exe
  MAP     =.map
  IDP     =.x64
  DLLEXT  =64x.wll
  IDP     =64.x64
  LDR     =64.x64
  PLUGIN  =.x64
  O       =.obj
  A       =.lib
  ifndef NORTL
    IDPSTUB =
    LDRSTUB =
    IDPSLIB =$(C_LIB)
  else
    IDPSTUB =$(LIBDIR)/modstart
    LDRSTUB =$(LIBDIR)/modstart
    IDPSLIB =$(C_LIB)
  endif
  AR      =$(RS)ar$(BS) _e.at _v _l$(MSVCDIR)bin/x86_amd64/lib.exe $(_LBSHOW) $(AROPT)
  # force C mode for .c files
  ifdef DONT_FORCE_CPP
    FORCEC=/TC
  endif
#############################################################################
else ifdef __CEARM__                                      # Visual C++ v4.0 for ARM 4.20
  BUILD_ONLY_SERVER=1
  COMPILER_NAME=vc
  TARGET_PROCESSOR_NAME=arm
  CC      ="$(MSVCARMDIR)bin/x86_arm/cl.exe"
  CFLAGS  =@$(IDA)$(CFGNAME) $(SWITCH64) $(PROCFLAG) $(NOWARNS) $(_CFLAGS) $(_CSHOW) # default compiler flags
  ##CFLAG_SUFFIX = /link /subsystem:windowsce
  OUTSW   =/Fe
  OBJSW   =/Fo
  ifdef BASE
    LDFLAGS =/BASE:$(BASE) $(_LDFLAGS)
  else
    LDFLAGS =$(_LDFLAGS)
  endif
  OUTDLL  =-LD
  LINKOPTS_EXE=/LIBPATH:"$(MSVCARMDIR)lib/armv4" /LIBPATH:"$(ARMSDK)lib/armv4"
  LINKOPTS=$(LINKOPTS_EXE) $(_LSHOW)
  _LINKER =$(LDEXE) _l$(MSVCARMDIR)bin/x86_arm/link.exe
  LINKER =$(_LINKER) $(LDFLAGS) $(LNDEBUG)
  CCL     =$(LDEXE) _c _l$(CC) _a"/link /subsystem:windowsce,4.20 /machine:arm /armpadcode $(CCL_LNK_OPT) /LIBPATH:/"$(MSVCARMDIR)lib/armv4/" /LIBPATH:/"$(ARMSDK)lib/armv4/"" $(CFLAGS) $(_LDFLAGS)
  C_LIB   =corelibc.lib coredll.lib
  B       =_arm.exe
  BS      =.exe
  MAP     =.mparm
  IDP     =.cearm32
  DLLEXT  =.dll
  IDP     =.cearm32
  LDR     =.cearm32
  PLUGIN  =.cearm32
  O       =.obj
  A       =.lib
  IDPSLIB =$(C_LIB)
  _LIBR   =$(MSVCARMDIR)bin/x86_arm/lib.exe
  AR      =$(RS)ar$(BS) _e.at _v "_l$(_LIBR)" $(_LBSHOW) $(AROPT)
  # force C mode for .c files
  ifdef DONT_FORCE_CPP
    FORCEC=/TC
  endif
  _ARMASM ="$(MSVCARMDIR)bin/x86_arm/armasm.exe"
  R32     =$(RS)/x86_win_vc/
  B32     =$(BS)
#############################################################################
else ifdef __VC__                                         # Visual Studio 2010 for x86
  TARGET_PROCESSOR_NAME=x86
  COMPILER_NAME=vc
  CC      =$(MSVCDIR)bin/cl.exe
  CFLAGS  =@$(IDA)$(CFGNAME) $(RUNTIME_LIBSW) $(SWITCH64) $(NOWARNS) $(_CFLAGS) $(_CSHOW) # default compiler flags
  ifndef LINK_ULINK
    OUTSW   =/Fe
    OBJSW   =/Fo
    BASESW  =/BASE
    OUTDLL  =/LD
    LNOUTDLL=/DLL
  else
    OUTSW   =-ZO
    OUJSW   =-j
    BASESW  =-b
    LNOUTDLL=-Tpd
    OUTDLL  =-Tpd
  endif
  ifdef BASE
    LDFLAGS =$(BASESW):$(BASE)
  endif
  _LIBRTL=$(MSVCDIR)lib
  LDFLAGS+=$(_LDFLAGS)
  ifdef LINK_NOBOR
    LINKOPTS_EXE=/LIBPATH:$(_LIBRTL) /LIBPATH:$(MSSDK)lib $(_LNK_EXE_MAP) $(LNDEBUG) /LARGEADDRESSAWARE /DYNAMICBASE
  else
    LINKOPTS_EXE=/L$(_LIBRTL);$(MSSDK)lib $(_LNK_EXE_MAP) $(LNDEBUG)
  endif
  LINKOPTS=$(LNOUTDLL) $(LINKOPTS_EXE) $(_LSHOW) $(LNDEBUG)
  ifdef LINK_ULINK
    _LINKER =$(ULINK)
    CCL     =$(LDEXE) _b _l$(ULINK) $(_ULCCOPT)
  else
    _LINKER =$(LDEXE) _l$(MSVCDIR)bin/link.exe
    CCL     =$(LDEXE) _v _l$(CC) _a"/link $(LINKOPTS_EXE) $(LDFLAGS)" $(CFLAGS)
  endif
  LINKER=$(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)
  C_STARTUP=
  C_LIB   =kernel32.lib
  B       =$(SUFF64).exe
  BS      =.exe
  MAP     =.map
  DLLEXT  =$(SUFF64).wll
  IDP     =$(SUFF64).w$(ADRSIZE)
  ifdef __EA64__
    LDR     =64.l$(ADRSIZE)
    PLUGIN  =.p$(ADRSIZE)
  else
    LDR     =.ldw
    PLUGIN  =.plw
  endif
  O       =.obj
  A       =.lib
  ifndef NORTL
    IDPSTUB =
    LDRSTUB =
    IDPSLIB =$(C_LIB)
  else
    IDPSTUB =$(LIBDIR)/modstart
    LDRSTUB =$(LIBDIR)/modstart
    IDPSLIB =$(C_LIB)
  endif
  AR      =$(RS)ar$(BS) _e.at _v _l$(MSVCDIR)bin/lib.exe $(_LBSHOW) $(AROPT)
  # force C mode for .c files
  ifdef DONT_FORCE_CPP
    FORCEC=/TC
  endif
#############################################################################
else                                                      # Borland C++ for NT (WIN32)
  TARGET_PROCESSOR_NAME=x86
  COMPILER_NAME=bcc
  CC      =$(BCB)/bin/bcc32.exe
  IMPLIB  =$(BCB)/bin/implib.exe
  ASM     =$(BC5_COM)/bin/tasm32.exe
  ifdef __PRECOMPILE__
    CC_PRECOMPILE= -H
  endif
  CFLAGS  =+$(IDA)$(CFGNAME) $(SWITCH64) $(CC_PRECOMPILE) -pr $(NOWARNS) $(_CFLAGS) $(_CSHOW)
  AFLAGS  =/D__FLAT__ /t/ml/m5$(_AFLAGS)
  ifndef LINK_ULINK
    OUTSW   =-n -e
  else
    OUTSW   =-ZO
  endif
  OBJSW   =-n. -o
  OUTDLL  =/Tpd
  ifdef BASE
    NT_BSW  =-b=$(BASE)
  endif
  LDFLAGS =$(NT_BSW) $(_LDFLAGS)
  ifdef LINK_ULINK
    _LINKER =$(ULINK)
    CCL     =$(LDEXE) _b _l$(ULINK) _a"$(C_LIB)" $(_ULCCOPT) $(C_STARTUP)
  else
    _LINKER =$(LDEXE) _l$(BCB)/bin/ilink32.exe -Gn
    CCL     =$(LDEXE) _b _l$(CC) $(CFLAGS) $(_LDFLAGS)
  endif
  LINKER=$(_LINKER) $(LDFLAGS) $(LNDEBUG) $(_LSHOW)
  C_STARTUP=c0x32
  C_IMP   =import32.lib
  C_LIB   =$(C_IMP) cw32mt.lib
  B       =$(SUFF64).exe
  BS      =.exe
  MAP     =.mpb
  IDP     =$(SUFF64).w$(ADRSIZE)
  DLLEXT  =$(SUFF64).wll
  ifdef __EA64__
    LDR     =64.l$(ADRSIZE)
    PLUGIN  =.p$(ADRSIZE)
  else
    LDR     =.ldw
    PLUGIN  =.plw
  endif
  ORDINALS= #-o                                           # import functions by ordinals
  # -c case sensitive
  # -C clear state before linking
  # -s detailed map of segments
  # -m detailed map of publics
  # -r verbose
  LINKOPTS_EXE= $(_LNK_EXE_MAP) -c -C $(ORDINALS) $(LNDEBUG)
  LINKOPTS=$(OUTDLL) $(LINKOPTS_EXE) $(_LSHOW)
  O       =.obj
  A       =.lib
  ifndef NORTL
    IDPSTUB =$(BCB)/lib/c0d32
    LDRSTUB =$(BCB)/lib/c0d32
    IDPSLIB =$(C_LIB)
  else
    IDPSTUB =$(LIBDIR)/modstart
    LDRSTUB =$(LIBDIR)/modstart
    IDPSLIB =$(C_IMP)
  endif
  AR      =$(RS)ar$(BS) _a _e.at "_l$(TLIB)" _f/C/E/P128 $(AROPT)
  # force C mode for .c files
  ifdef DONT_FORCE_CPP
    FORCEC=-P-
  endif
endif
#############################################################################

ifdef __CEARM__
  # put all wince binaries in vc directory because otherwise
  # ida does not see them and can not update the wince device
  BINDIR=x86_win_vc
else
  BINDIR=$(TARGET_PROCESSOR_NAME)_$(SYSNAME)_$(COMPILER_NAME)$(OPTSUF)$(STATSUF)
endif

SYSDIR=$(TARGET_PROCESSOR_NAME)_$(SYSNAME)_$(COMPILER_NAME)_$(ADRSIZE)
# libraries directory
LIBDIR=$(IDA)lib/$(SYSDIR)
# object files directory
OBJDIR=obj/$(SYSDIR)$(OPTSUF)$(STATSUF)
# PDB files directory
PDBDIR=$(IDA)pdb/$(SYSDIR)
CFGNAME=$(SYSDIR)$(OPTSUF)$(STATSUF).cfg

ifeq (1,0)           # this is for makedep
  F=
else
  F=$(OBJDIR)/
  L=$(LIBDIR)/
  R=$(IDA)bin/
endif

RS=$(IDA)bin/
BS=.exe

ifndef R32
  # can be defined before for build x64 in win32
  R32=$(R)
  B32=$(B)
else
  B32=$(BS)
endif

# Help Compiler
HC=$(R32)ihc$(B32)
STM=$(R32)stm$(B32)

IDALIB:=$(L)ida$(A)
DUMB:=$(L)dumb$(O)
HLIB:=$(HI)_ida.hlp

CLPLIB:=$(L)clp$(A)

RM=rm -f
CP=cp -f
MV=mv
MKDIR=-@mkdir

########################################################################
CONLY?=-c

$(F)%$(O): %.cpp | objdir
ifneq ($(or $(__ANDROID__),$(__ARMLINUX__)),)
	$(CCX) $(CPPFLAGS) $(CONLY) $(OBJSW)$@ $<
$(F)%$(O): %.c | objdir
	$(CC) $(CFLAGS) $(CONLY) $(OBJSW)$@ $<
else
	$(CC) $(CFLAGS) $(CONLY) $<
$(F)%$(O): %.c | objdir
	$(CC) $(CFLAGS) $(CONLY) $(FORCEC) $<
$(F)%$(O): %.asm | objdir
	$(ASM) $(AFLAGS) $(call dospath,$<),$(call dospath,$@)
endif

%.hhp: %.hls
	$(HC) -t $(HLIB) -i$@ $?
########################################################################
endif # if unix or windows