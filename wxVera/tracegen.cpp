/** Trace generation script
 **
 ** Written by Danny Quist, <dquist@lanl.gov
 ** 
 ** Copyright (C) 2011, Los Alamos National Security, All Rights Reserved.
 ** No use without explicit written permission
 **/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "Trace.h"

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage: %s [trace file] [original executable] [GML file to write to]\n", argv[0]);
		return -1;
	}

	Trace *t = NULL;

	try
	{ 
		t = new Trace(argv[1], argv[2], argv[3]);

		// Parse the trace file 
		t->process(false);

		if (t->edgeMap.size() == 0)
			throw "Failed, no edges in graph\n";

		if (t->bblMap.size() == 0)
			throw "Failed, no vertices in graph\n";


		// Write the GML file out
		t->writeGmlFile(argv[3]);

		// Run the layout algorithm on the newly written graph
		t->layoutGraph(argv[3]);

		delete t;

		
	}
	catch (char *e)
	{
		fprintf(stderr, "FAILED: %s\n", e);
		return -1;
	}
	     
	
	return 0;
}
