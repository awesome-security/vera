#ifndef __IGRAPH_TRACE_H__
#define __IGRAPH_TRACE_H__

#include "trace.h"

class IgraphTrace : public Trace
{
public:
	IgraphTrace(char *tracefile, char *orig_exe_file, char *outputfile) 
		: Trace(tracefile, orig_exe_file, outputfile){}
	
	IgraphTrace(wxString tracefile, wxString orig_exe_file, wxString outputfile)
		: Trace(tracefile, orig_exe_file, outputfile) {}
	
	IgraphTrace() 
		:Trace() {}

	~IgraphTrace(void);

	// The virtual function override for the Trace class
	void layoutGraph(const char *infile, const char *outfile);
};

#endif