#include "wxvera.h"

threadTraceBuilder::threadTraceBuilder(wxString traceFile, 
				       wxString exeFile, 
				       wxString gmlSaveFile, 
				       bool doBbl, 
				       bool doAll,
				       int graphLayoutAlgorithm,
				       wxFrame *parentFrame,
				       wxProgressDialog *prog)
{
	m_traceFile = traceFile;

	if (exeFile.length() == 0)
		m_exeFile.Clear();
	else
		m_exeFile = exeFile;
	
	m_gmlSaveFile = gmlSaveFile;
        // Due to this bug with wxWidgets: http://trac.wxwidgets.org/ticket/4272
	// No progress updates on the Mac
#ifdef __APPLE__
	m_prog = NULL;
#else
	m_prog = prog;
#endif
	m_doBbl = doBbl;
	m_doAll = doAll;
	m_parentFrame = parentFrame;

	m_graphLayoutAlgorithm = graphLayoutAlgorithm;

}

Trace *threadTraceBuilder::allocateTraceClass(wxString outfilename)
{
	Trace *ret = NULL;

	switch (this->m_graphLayoutAlgorithm)
	{
	case GRAPH_LAYOUT_LIBRARY_IGRAPH:
		//ret = new IgraphTrace(m_traceFile.GetFullPath(), m_exeFile.GetFullPath(), outfilename);
		//break;
	case GRAPH_LAYOUT_LIBRARY_UNSPECIFIED:
	case GRAPH_LAYOUT_LIBRARY_OGDF:
		ret = new OgdfTrace(m_traceFile.GetFullPath(), m_exeFile.GetFullPath(), outfilename);
		break;
	case GRAPH_LAYOUT_LIBRARY_INVALID:
	default:
		throw "Invalid layout library specified";
	}
	
	return ret;
}

void *threadTraceBuilder::Entry()
{

#ifdef __APPLE__
	wxString pathToTracegen = wxString::Format("%s%s%s",
						   wxPathOnly(wxStandardPaths::Get().GetExecutablePath()).c_str(),
						   wxFileName::GetPathSeparators().c_str(),
						   wxT(TRACEGEN_CMD));
						   
	wxString bblOpt = wxT("");

	wxString exe = (m_exeFile.IsOk()) ?
		wxString::Format(wxT("-e %s"), m_exeFile.GetFullPath().c_str()):
		wxT("") ;

	if (m_doBbl)
	{
		wxString outfilename = prependFileName(m_gmlSaveFile, wxT("bbl-"));
		wxString cmd = wxString::Format(wxT("%s -t %s %s -o %s %s"),
						pathToTracegen.c_str(),
						m_traceFile.GetFullPath().c_str(),
						exe.c_str(),
						outfilename.c_str(),
						bblOpt.c_str());
		
		system(cmd.c_str());
	}

	if (m_doAll)
	{
		wxString outfilename = prependFileName(m_gmlSaveFile, wxT("all-"));
		wxString cmd = wxString::Format(wxT("%s -t %s %s -o %s %s"),
						pathToTracegen.c_str(),
						m_traceFile.GetFullPath().c_str(),
						exe.c_str(),
						outfilename.c_str(),
						bblOpt.c_str());
		
		system(cmd.c_str());
	}
		
#else
	try 
	{

		// Process the basic blocks
		if (m_doBbl)
		{
			// Add "bbl-" to the beginning of the file name
			wxString outfilename = prependFileName(m_gmlSaveFile, wxT("bbl-"));
			wxString tmpfilename = prependFileName(m_gmlSaveFile, wxT("tmp-bbl-"));
			
			//Trace *t = allocateTraceClass(outfilename);
			Trace *t = allocateTraceClass(outfilename);
			
			if (t == NULL)
			{
				//wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
				throw "Could not allocate memory for Trace object";
			}

			t->process(true); // Process basic blocks

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(10);
				wxMutexGuiLeave();
			}

			t->writeGmlFile(tmpfilename);

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(20);
				wxMutexGuiLeave();
			}

			t->layoutGraph(tmpfilename);
			t->writeExecutionOrder(tmpfilename);

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(30);
				wxMutexGuiLeave();
			}

			delete t;
			
			if (!wxRemoveFile(tmpfilename))
			{
				wxMutexGuiEnter();
				wxLogDebug(wxT("Could not remove temp file"));
				wxMutexGuiLeave();
			}
		}

		if (m_prog) 
		{
			wxMutexGuiEnter();
			m_prog->Update(50);
			wxMutexGuiLeave();
		}

		// Process all addresses
		if (m_doAll)
		{
			// Add "all-" to the beginning of the file name
			wxString outfilename = prependFileName(m_gmlSaveFile, wxT("all-"));
			wxString tmpfilename = prependFileName(m_gmlSaveFile, wxT("tmp-all-"));

			Trace *t = allocateTraceClass(outfilename);
			
			if (t == NULL)
			{
				wxString err(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
				throw err.c_str();
			}

			t->process(false); // Process basic blocks

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(60);
				wxMutexGuiLeave();
			}

			t->writeGmlFile(tmpfilename);

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(80);
				wxMutexGuiLeave();
			}

			t->layoutGraph(tmpfilename);
			t->writeExecutionOrder(outfilename);

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(90);
				wxMutexGuiLeave();
			}

			delete t;

			if (!wxRemoveFile(tmpfilename))
			{
				wxMutexGuiEnter();
				wxLogDebug(wxT("Could not remove temp file"));
				wxMutexGuiLeave();
			}
		}
	} // end try
	catch (char *e)
	{
		wxCommandEvent ErrEvt( wxEVT_COMMAND_BUTTON_CLICKED );
		ErrEvt.SetInt(THREAD_TRACE_ERROR);
		ErrEvt.SetString(wxString(e));

		if (m_parentFrame)
		{
			wxMutexGuiEnter();
			wxPostEvent(m_parentFrame, ErrEvt);
			wxMutexGuiLeave();
		}

		wxMutexGuiEnter();
		wxLogDebug(wxString::Format(wxT("Error processing trace: %s"), e));
		wxMutexGuiLeave();

		delete e;
		
		return NULL;
	}
#endif // ifdef __APPLE__

	if (m_prog)
	{
		wxMutexGuiEnter();
		m_prog->Update(100);
		wxMutexGuiLeave();
	}

	if (m_prog)
	{
		wxMutexGuiEnter();
		m_prog->Destroy();
		wxMutexGuiLeave();
	}

	// Tell the parent frame we're done so it can load the GUI
	wxCommandEvent DoneEvent( wxEVT_COMMAND_BUTTON_CLICKED );

	// Put the save file name in the event for later display 
	DoneEvent.SetString(m_gmlSaveFile.GetFullPath());

	// Set the flags so the receiving event knwos what's going on
	if (m_doBbl && m_doAll)
		DoneEvent.SetInt(THREAD_TRACE_BOTH_PROCESSED);
	else if (m_doBbl && !m_doAll)
		DoneEvent.SetInt(THREAD_TRACE_BASIC_BLOCKS_PROCESSED);
	else if (!m_doBbl && m_doAll)
		DoneEvent.SetInt(THREAD_TRACE_ALL_ADDRESSES_PROCESSED);
	else
		DoneEvent.SetInt(THREAD_TRACE_NONE_PROCESSED);
	
	if (m_parentFrame)
	{
		wxMutexGuiEnter();
		wxPostEvent(m_parentFrame, DoneEvent);
		wxMutexGuiLeave();
	}

	return NULL;
}

void threadTraceBuilder::OnExit(void)
{
	// Called when the thread exits by termination or with Delete()
	// but not Kill()ed
	
	// We don't really have anything to do here
}

threadTraceBuilder::~threadTraceBuilder(void)
{
}
