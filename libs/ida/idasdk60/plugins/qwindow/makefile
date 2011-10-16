# undef ms windows this plugin can be built only with visual studio
!ifdef __VC__
BUILD_QWINDOW=1
!ifdef __X64__
!undef __X64__
__EA64__=1
!endif
!endif

!ifndef __NT__
!ifndef __X64__
BUILD_QWINDOW=1
!endif
!endif

!ifdef BUILD_QWINDOW
PROC=qwindow
__CFLAGS=-I"$(QTDIR)include\QtCore" \
         -I"$(QTDIR)include\QtGui"  \
         -I"$(QTDIR)include\QtXml"  \
         -I"$(QTDIR)include"        \
         -I.                        \
         -DUNICODE                  \
         -DQT_DLL                   \
         -DQT_XML_LIB               \
         -DQT_GUI_LIB               \
         -DQT_CORE_LIB              \
         -DQT_NAMESPACE=QT          \
         -DQT_THREAD_SUPPORT

NO_STDCALL=1 # Qt must be compiled with __cdecl because of some QString helper functions

!ifdef __MAC__
PREF=$(IDA)idaq.app/Contents/Frameworks/
ADDITIONAL_LIBS="$(PREF)QtXml.framework/QtXml"  \
                "$(PREF)QtGui.framework/QtGui"  \
                "$(PREF)QtCore.framework/QtCore"
!else
!ifdef __LINUX__
PREF=$(R)lib
POST=.so.4
!endif
!ifdef __NT__
PREF=$(IDA)lib\qt.w32\ # with backslash
POST=4$(A)
!endif
ADDITIONAL_LIBS="$(PREF)QtXml$(POST)"  \
                "$(PREF)QtGui$(POST)"  \
                "$(PREF)QtCore$(POST)"
!endif
O1=moc_myactions

!include ..\plugin.mak

$(F)moc_myactions$(O): myactions.h
	$(QTDIR)bin\moc -I. myactions.h >$(F)moc_myactions.cpp
	$(CC) $(CFLAGS) -c $(OBJSW)$@ $(F)moc_myactions.cpp

# MAKEDEP dependency list ------------------
$(F)qwindow$(O) : $(I)area.hpp $(I)bytes.hpp $(I)fpro.h $(I)funcs.hpp       \
	          $(I)ida.hpp $(I)idp.hpp $(I)kernwin.hpp $(I)lines.hpp     \
	          $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp                 \
	          $(I)netnode.hpp $(I)pro.h $(I)segment.hpp $(I)ua.hpp      \
	          $(I)xref.hpp qwindow.cpp
!else
!ifndef __NT__
all:
!endif
!endif