/*
//      Written by Nathan Brown and Danny Quist
//
//      Copyright (C) 2011 Los Alamos National Security, All rights reserved.
//      No use permitted without explicit written permission.
//
//	tracegen.cpp
//	a driver for generating graphs from ether output trace
//	using code from Vera.
//
//	main()
//
//	execute:  tracegen -t .trace -e .exe -o output [-b ] 
//	(-b = process basic blocks)
//
*/

#include <iostream>	// cout
#include <string>	// string
#include <stdio.h>	// remove()
#include <unistd.h>	// getopt()

#include "wxvera.h"
#include "Trace.h"

using namespace std;

void usage(void)
{
	fprintf(stderr, "Usage: tracegen "
		"[-t tracefile] "
		"[-e executable (PE only)] "
		"[-o GML output file] "
		"[-b process basic blocks] "
		"[-v prints version] "
		"\n");

}

void version(void)
{
	fprintf(stderr, "tracegen utility from VERA " __VERA_VERSION__ "\n");
}

int main(int argc, char **argv)	// add command line inputs argv/argc later
{
	int c;

	char *topt = 0;
	char *eopt = 0;
	char *oopt = 0;
	int bopt = 0;

	wxString input_trace;
	wxString input_executable;
	wxString output_finalgraph;
	wxString output_graph;

	bool missing_arguments = false;

	while ( (c = getopt(argc, argv, "t:e:o:bv")) != -1)
	{
		switch (c)
		{
		case 't': // Trace file
			topt = optarg;
			break;
		case 'e': // executable file to use
			eopt = optarg;
			break;
		case 'o': // Output GML file
			oopt = optarg;
			break;
		case 'b': // Process basic blocks only
			bopt = 1;
			break;
		case 'v': // Print the version
			version();
			exit(0);
		default:
			printf("Bad input\n");
			usage();
			exit(1);
		}
	}

	if (topt != 0)
		input_trace = wxString(topt);
	if (eopt != 0)
		input_executable = wxString(eopt);
	if (oopt != 0)
		output_finalgraph = wxString(oopt);

	// This should be fixed to not collide with multiple running versions
	output_graph = wxFileName::CreateTempFileName(wxT("tracegen-"));

	bool printedUsage = false;
	
	if (input_trace.length() == 0)
	{
		if (!printedUsage)
		{
			usage();
			printedUsage = true;
		}

		fprintf(stderr, "ERROR: No tracefile specified, use -t tracefile\n");
		missing_arguments = true;
	}

	if (input_executable.length() == 0)
	{
		fprintf(stderr, "WARNING: No executable specified. Use -e to specify executable. Output file will be missing features\n");
	}

	if (output_finalgraph.length() == 0)
	{
		if (!printedUsage)
		{
			usage();
			printedUsage = true;
		}
		
		fprintf(stderr, "ERROR: No output graph filename not specified, use -o graph.gml\n");
		missing_arguments = true;
	}

	if (missing_arguments)
	{
		if (!printedUsage)
		{
			usage();
			printedUsage = true;
		}

		fprintf(stderr, "ERROR: missing arguments\n");
		return 1;
	}


	// perform the trace generation
	try
	{
		Trace* thetrace	= new OgdfTrace(input_trace.c_str(), input_executable.c_str(), output_graph.c_str());
		
		// only process basic blocks if command line option was passed
		if (bopt)
		{
			thetrace->process(true);
		}
		else
		{
			thetrace->process(false);
		}
		
		thetrace->writeGmlFile();	// default with no parameters writes to file passed to Trace constructor
		thetrace->layoutGraph(output_graph.c_str(), output_finalgraph.c_str());
		thetrace->writeExecutionOrder(output_finalgraph);
		
		delete thetrace;
	}
	catch (wxString e)
	{
		fprintf(stderr, "ERROR: %s\n", e.c_str());
	}
	
	// it is safe to delete output_graph file now
	if( remove(output_graph.c_str()) != 0)
	{
		fprintf(stderr, "Error while deleting temporary graph file %s\n", output_graph.c_str());
		return 1;
	}

	return 0;
}
