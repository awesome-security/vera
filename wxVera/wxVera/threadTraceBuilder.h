#ifndef __THREAD_TRACE_BUILDER_H__
#define __THREAD_TRACE_BUILDER_H__

//#include "wxvera.h"
#include "Trace.h"
#include "OgdfTrace.h"
//#include "IgraphTrace.h"

using namespace std;

class Trace;

class threadTraceBuilder :
	public wxThread
{
public:
	threadTraceBuilder(wxString traceFile, 
					   wxString exeFile, 
					   wxString gmlSaveFile, 
					   bool doBbl, 
					   bool doAll,
					   int graphLayoutAlgorithm=GRAPH_LAYOUT_LIBRARY_OGDF,
					   wxFrame *parentFrame=NULL
					   );

	~threadTraceBuilder(void);

	virtual void *		Entry();
	virtual void		OnExit();

private:
	wxFrame *			m_parentFrame;
	wxFileName 			m_traceFile;
	wxFileName 			m_exeFile;
	wxFileName 			m_gmlSaveFile;
	bool 				m_doBbl;
	bool 				m_doAll;
	bool				m_doProcessExe;
	int					m_graphLayoutAlgorithm;
	Trace *				allocateTraceClass(wxString);
	void                            updateProgress(int);

};

#endif
