UNAME := $(shell uname)

# Linux
ifeq ($(UNAME), Linux)
CC=/usr/bin/g++
FLAGS=`/usr/bin/wx-config --cppflags` -g -D_USE_WX_EVENTS_ -Wno-write-strings -Wno-deprecated \
	-I/usr/include/freetype2 -I../../ogdf/linux
OGDF=../../ogdf/linux/ogdf/libOGDF.a
LIBS=`/usr/bin/wx-config --libs` -lglut -lfreetype -lpthread -lwx_gtk2u_gl-2.8 
endif

# Mac OS X
ifeq ($(UNAME), Darwin)
CC=/usr/bin/g++
FLAGS=`/usr/local/bin/wx-config --cppflags` -cpp-precomp -g -arch i386 -D_USE_WX_EVENTS_ -Wno-write-strings -Wno-deprecated \
	-I/usr/local/include/freetype2 -I../../ogdf/mac
LIBS=-L../../ogdf/mac/ogdf `/usr/local/bin/wx-config --libs` -lwx_mac_gl-2.8 -framework GLUT -lfreetype -lOGDF
endif

all: wxvera tests tracegen

poop:
	@echo $(FLAGS) $(LIBS)

wxvera: util.o FreeType.o Config.o ConfigFrame.o LegendFrame.o ThreadTraceBuilder.o \
	Viz.o VizFrame.o Trace.o TraceWizard.o wxvera.o #ThreadIdaServer.o
	$(CC) $(FLAGS) $(LIBS) util.o FreeType.o Config.o ConfigFrame.o LegendFrame.o \
	Viz.o VizFrame.o ThreadTraceBuilder.o TraceWizard.o Trace.o wxvera.o $(OGDF) -o wxvera

tracegen: util.o Trace.o tracegen.cpp tracegen.h
	$(CC) $(FLAGS) $(LIBS) util.o Trace.o tracegen.cpp $(OGDF) -o tracegen

tests: testTrace

testTrace: testTrace.o Trace.o util.o
	$(CC) $(FLAGS) $(INCLUDE) $(LIBS) util.o Trace.o testTrace.o $(OGDF) -o testTrace

testTrace.o: testTrace.cpp testTrace.h
	$(CC) $(FLAGS) $(INCLUDE) -c testTrace.cpp -o testTrace.o

wxvera.o: wxvera.cpp wxvera.h
	$(CC) $(FLAGS) $(INCLUDE) -c wxvera.cpp -o wxvera.o

VizFrame.o: VizFrame.cpp VizFrame.h
	$(CC) $(FLAGS) $(INCLUDE) -c VizFrame.cpp -o VizFrame.o

Viz.o: Viz.cpp Viz.h
	$(CC) $(FLAGS) $(INCLUDE) -c Viz.cpp -o Viz.o

util.o: util.h util.cpp
	$(CC) $(FLAGS) $(INCLUDE) -c util.cpp -o util.o

FreeType.o: FreeType.cpp FreeType.h
	$(CC) $(FLAGS) $(INCLUDE) -c FreeType.cpp -o FreeType.o

Config.o: Config.cpp Config.h
	$(CC) $(FLAGS) $(INCLUDE) -c Config.cpp -o Config.o

ConfigFrame.o: configFrame.cpp configFrame.h
	$(CC) $(FLAGS) $(INCLUDE) -c configFrame.cpp -o ConfigFrame.o 

LegendFrame.o: LegendFrame.cpp LegendFrame.h
	$(CC) $(FLAGS) $(INCLUDE) -c LegendFrame.cpp -o LegendFrame.o

ThreadIdaServer.o: threadIdaServer.cpp threadIdaServer.h
	$(CC) $(FLAGS) $(INCLUDE) -c threadIdaServer.cpp -o ThreadIdaServer.o

ThreadTraceBuilder.o: threadTraceBuilder.cpp threadTraceBuilder.h
	$(CC) $(FLAGS) $(INCLUDE) -c threadTraceBuilder.cpp -o ThreadTraceBuilder.o

TraceWizard.o: traceWizard.cpp traceWizard.h
	$(CC) $(FLAGS) $(INCLUDE) -c traceWizard.cpp -o TraceWizard.o

Trace.o: Trace.cpp Trace.h
	$(CC) $(FLAGS) $(INCLUDE) -c Trace.cpp -o Trace.o

clean:
	rm -rf *.o wxvera tracegen testTrace *~ unittest/*gml
