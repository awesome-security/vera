RS=$(IDA)bin/

PEUTIL=$(RS)peutil.exe

ULNK_BASE=$(RS)ulink.exe

TLIB=$(RS)tlib.exe

ULNK_CFG_DIR=$(RS)

NASM=$(RS)nasmw.exe

#-------------------------------------------------------
BCB?=c:/progra~2/borland/cbuild~1
MSSDK?=C:/PROGRA~1/MICROS~3/WINDOWS/v6.0A/
VSPATH?=C:/PROGRA~2/MICROS~2.0/
MSVCDIR?=$(VSPATH)VC/
VSPATH8?=C:/PROGRA~2/MICROS~1.0/
MSVCARMDIR?=$(VSPATH8)VC/ce/
ARMSDK?=$(VSPATH8)SmartDevices/SDK/PocketPC2003/
GCCBINDIR?=c:/cygwin/bin
PYTHON_ROOT?=c:
PYTHON?=$(PYTHON_ROOT)/python26/python.exe
SWIG?=z:/swig/swig.exe
QT_ROOT?=c:
QTDIR?=$(QT_ROOT)/Qt/4.7.2/
ANDROID_NDK?=c:/android-ndk-r4b/
SOURCERY?=C:/CODESO~1/SOURCE~1
STLDIR?=z:/idasrc/third_party/stlport
HHC?=$(IDA)ui/qt/help/chm/hhc.exe

# keep all paths in dos format, with double backslashes
BCB          :=$(call unixpath,$(BCB))
MSSDK        :=$(call unixpath,$(MSSDK))
VSPATH8      :=$(call unixpath,$(VSPATH8))
VSPATH       :=$(call unixpath,$(VSPATH))
MSVCDIR      :=$(call unixpath,$(MSVCDIR))
MSVCARMDIR   :=$(call unixpath,$(MSVCARMDIR))
ARMSDK       :=$(call unixpath,$(ARMSDK))
GCCBINDIR    :=$(call unixpath,$(GCCBINDIR))
PYTHON_ROOT  :=$(call unixpath,$(PYTHON_ROOT))
PYTHON       :=$(call unixpath,$(PYTHON))
SWIG         :=$(call unixpath,$(SWIG))
QT_ROOT      :=$(call unixpath,$(QT_ROOT))
QTDIR        :=$(call unixpath,$(QTDIR))
ANDROID_NDK  :=$(call unixpath,$(ANDROID_NDK))
SOURCERY     :=$(call unixpath,$(SOURCERY))
STLDIR       :=$(call unixpath,$(STLDIR))
HHC          :=$(call unixpath,$(HHC))

################################EOF###############################
