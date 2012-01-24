#ifndef __OGDF_TRACE_H__
#define __OGDF_TRACE_H__

#include "Trace.h"

class OgdfTrace : public Trace
{
public:
	OgdfTrace(char *tracefile, char *orig_exe_file, char *outputfile) 
		: Trace(tracefile, orig_exe_file, outputfile){}
	
	OgdfTrace(wxString tracefile, wxString orig_exe_file, wxString outputfile)
		: Trace(tracefile, orig_exe_file, outputfile) {}
	
	OgdfTrace() 
		:Trace() {}

	~OgdfTrace(void);

	// The virtual function override for the Trace class
	void layoutGraph(const char *infile, const char *outfile);
};

#endif
