
!include ../../../allmake.mak

all:    objdir rapitest.exe rapi_arm.dll

run:
        rapitest.exe

rapi_arm.dll: rapi_arm.cpp ..\async.cpp
        clce -I../../../include -D__NT__ /GX /LD rapi_arm.cpp \
        ..\..\..\lib\cearm.l32\pro.lib ..\..\..\lib\cearm.l32\dumb.obj

rapitest.exe: $(F)rapitest$(O) ..\async.cpp $(L)pro$(A) $(DUMB)
        $(CCL) $(OUTSW)$@ $(F)rapitest$(O) $(L)pro$(A) $(DUMB)

!include ../../../objdir.mak

# MAKEDEP dependency list ------------------
$(F)rapitest$(O): $(I)area.hpp $(I)bytes.hpp $(I)diskio.hpp $(I)err.h       \
	          $(I)fpro.h $(I)funcs.hpp $(I)help.h $(I)ida.hpp           \
	          $(I)idd.hpp $(I)idp.hpp $(I)kernwin.hpp $(I)lines.hpp     \
	          $(I)llong.hpp $(I)nalt.hpp $(I)netnode.hpp $(I)pro.h      \
	          $(I)segment.hpp $(I)ua.hpp $(I)xref.hpp ../async.cpp      \
	          ../idarpc.hpp ../rapi/rapi.h ceapimap.h rapitest.cpp

