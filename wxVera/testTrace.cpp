#include <stdio.h>
#include <stdint.h>

#include "testTrace.h"
#include "Trace.h"
#include "util.h"


int main (int argc, char **argv)
{
	wxString gmlSaveFile(wxT(TEST_OUTPUT_FILE));

	Trace *t = NULL;
	
	wxString exefile(wxT(TEST_EXE_FILE));
	wxString tracefile(wxT(TEST_TRACE_FILE));
	
	wxString bbl_outfilename = prependFileName(gmlSaveFile, wxT("bbl-"));
	wxString bbl_tmpfilename = prependFileName(gmlSaveFile, wxT("tmp-bbl-"));

	wxString all_outfilename = prependFileName(gmlSaveFile, wxT("all-"));
	wxString all_tmpfilename = prependFileName(gmlSaveFile, wxT("tmp-all-"));

	printf("Trace allocation and initialization...");
	fflush(stdout);

	try
	{
		t = new Trace(tracefile,
			      exefile,
			      bbl_outfilename);
	}
	catch(char *e)
	{
		printf("FAILED %s\n", e);
		return 1;
	}
	
	if (t == NULL)
	{
		printf("FAILED allocation!\n");
		return 2;
	}

	printf("passed.\n");

	// Test basic blocks first

	printf("Trace basic block processing...");
	fflush(stdout);
	
	try
	{
		t->process(true);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 3;
	}

	// Check the number of nodes

	if (t->edgeMap.size() == 0)
	{
		printf("FAILED! no edges in graph\n");
		return 3;
	}

	if ( t->bblMap.size() == 0 )
	{
		printf("FAILED! no vertices in graph\n");
		return 3;
	}

	printf("passed\n");


	printf("Testing writing of temporary file...");
	fflush(stdout);

	try
	{
		t->writeGmlFile(bbl_tmpfilename);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 5;
	}

	printf("passed\n");

	
	printf("Testing graph layout...");
	fflush(stdout);

	try
	{
		t->layoutGraph(bbl_tmpfilename);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 6;
	}

	printf("passed\n");
	
	wxRemoveFile(bbl_tmpfilename);
	
	// Test all blocks

	printf("Trace allocation and initialization...");
	fflush(stdout);

	try
	{
		t = new Trace(tracefile,
			      exefile,
			      all_outfilename);
	}
	catch(char *e)
	{
		printf("FAILED %s\n", e);
		return 1;
	}

	printf("Trace all block processing...");
	fflush(stdout);

	try
	{
		t->process(false);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 4;
	}

	printf("passed\n");


	printf("Testing writing of temporary file...");
	fflush(stdout);

	try
	{
		t->writeGmlFile(all_tmpfilename);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 5;
	}

	printf("passed\n");

	printf("Testing graph layout...");
	fflush(stdout);

	try
	{
		t->layoutGraph(all_tmpfilename);
	}
	catch (char *e)
	{
		printf("FAILED! %s\n", e);
		return 6;
	}

	printf("passed\n");

	wxRemoveFile(all_tmpfilename);
	
	return 0;
}
