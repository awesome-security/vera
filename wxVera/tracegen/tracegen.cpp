// tracegen.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include "Trace.h"

int _tmain(int argc, _TCHAR* argv[])
{
	char tracefile	[FILELEN] = {0};
	char exefile	[FILELEN] = {0};
	char gmlfile	[FILELEN] = {0};

	if (argc < 3 || argc > 4)
	{
		printf("Usage: tracegen.exe [trace file] [original executable] ([optional GML output file])\n");
		return -1;
	}

	strncpy_s(tracefile,	sizeof(tracefile),	argv[1],	sizeof(tracefile));
	strncpy_s(exefile,		sizeof(exefile),	argv[2],	sizeof(exefile));

	if (argc == 4) // GML output file was specified
	{
		strncpy_s(gmlfile, sizeof(gmlfile), argv[3], sizeof(gmlfile));
	}
	else
	{
		sprintf_s(gmlfile, sizeof(gmlfile), "%s.gml", tracefile);
	}

	printf ("Preparing trace from %s with exe %s into %s\n", tracefile, exefile, gmlfile);

	Trace *t = new Trace(tracefile, exefile, gmlfile);

	t->process(true);

	t->writeGmlFile();

	
	return 0;
}

