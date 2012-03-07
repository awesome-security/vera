#include "wxvera.h"

threadTraceBuilder::threadTraceBuilder(wxString traceFile, 
				       wxString exeFile, 
				       wxString gmlSaveFile, 
				       bool doBbl, 
				       bool doAll,
				       int graphLayoutAlgorithm,
				       wxFrame *parentFrame)
{
	m_traceFile = traceFile;

	if (exeFile.length() == 0)
		m_exeFile.Clear();
	else
		m_exeFile = exeFile;
	
	m_gmlSaveFile = gmlSaveFile;

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

/*
 * Helper to post commands to update the GUI
 */
void threadTraceBuilder::updateProgress(int pctDone)
{
	wxCommandEvent DoneEvent( wxEVT_COMMAND_BUTTON_CLICKED );

	// Should be value from 0 -> 100
	if (pctDone < 0 || pctDone > 100)
	{
		wxLogDebug(wxT("Invalid percentage completed %d"), pctDone);
		return;
	}

	DoneEvent.SetInt(THREAD_TRACE_UPDATE_PROGRESS);
	DoneEvent.SetString(wxString::Format("%d", pctDone));

	wxMutexGuiEnter();
	wxPostEvent(m_parentFrame, DoneEvent);
	wxMutexGuiLeave();
	
}

void *threadTraceBuilder::Entry()
{

#ifdef __APPLE__
	wxString pathToTracegen = wxString::Format("%s%s%s",
						   wxPathOnly(wxStandardPaths::Get().GetExecutablePath()).c_str(),
						   wxFileName::GetPathSeparators().c_str(),
						   wxT(TRACEGEN_CMD));

	// Check to see that the executable has been specified. If it hasn't, then just leave
	// it blank
	wxString exe = (m_exeFile.IsOk()) ?
		wxString::Format(wxT("-e %s"), m_exeFile.GetFullPath().c_str()):
		wxT("") ;

	if (m_doBbl)
	{
		wxString outfilename = prependFileName(m_gmlSaveFile, wxT("bbl-"));
		wxString cmd = wxString::Format(wxT("%s -t %s %s -o %s -b"),
						pathToTracegen.c_str(),
						m_traceFile.GetFullPath().c_str(),
						exe.c_str(),
						outfilename.c_str());
		
		system(cmd.c_str());
		updateProgress(30);
	}

	updateProgress(50);

	if (m_doAll)
	{
		wxString outfilename = prependFileName(m_gmlSaveFile, wxT("all-"));
		wxString cmd = wxString::Format(wxT("%s -t %s %s -o %s"),
						pathToTracegen.c_str(),
						m_traceFile.GetFullPath().c_str(),
						exe.c_str(),
						outfilename.c_str());
		
		system(cmd.c_str());
		updateProgress(90);
	}

	updateProgress(100);
		
#else
	Trace *t = NULL;
	try 
	{

		// Process the basic blocks
		if (m_doBbl)
		{
			// Add "bbl-" to the beginning of the file name
			wxString outfilename = prependFileName(m_gmlSaveFile, wxT("bbl-"));
			wxString tmpfilename = prependFileName(m_gmlSaveFile, wxT("tmp-bbl-"));
			
			t = allocateTraceClass(outfilename);
			
			if (t == NULL)
				throw "Could not allocate memory for Trace object";

			t->process(true); // Process basic blocks

			updateProgress(10);

			t->writeGmlFile(tmpfilename);

			updateProgress(20);


			t->layoutGraph(tmpfilename);
			t->writeExecutionOrder(tmpfilename);

			updateProgress(30);

			delete t;
			t = NULL;
			
			if (!wxRemoveFile(tmpfilename))
			{
				wxMutexGuiEnter();
				wxLogDebug(wxT("Could not remove temp file"));
				wxMutexGuiLeave();
			}
		}

		updateProgress(50);

		// Process all addresses
		if (m_doAll)
		{
			// Add "all-" to the beginning of the file name
			wxString outfilename = prependFileName(m_gmlSaveFile, wxT("all-"));
			wxString tmpfilename = prependFileName(m_gmlSaveFile, wxT("tmp-all-"));

			t = allocateTraceClass(outfilename);
			
			if (t == NULL)
			{
				wxString err(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
				throw err.c_str();
			}

			t->process(false); // Process basic blocks

			updateProgress(60);

			t->writeGmlFile(tmpfilename);

			updateProgress(80);

			t->layoutGraph(tmpfilename);
			t->writeExecutionOrder(outfilename);

			updateProgress(90);

			delete t;
			t = NULL;

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
		
		if (t)
		{
			delete t;
			t = NULL;
		}
		
		return NULL;
	}
	catch (wxString e)
	{
		wxCommandEvent ErrEvt( wxEVT_COMMAND_BUTTON_CLICKED );
		ErrEvt.SetInt(THREAD_TRACE_ERROR);
		ErrEvt.SetString(e);

		if (m_parentFrame)
		{
			wxMutexGuiEnter();
			wxPostEvent(m_parentFrame, ErrEvt);
			wxMutexGuiLeave();
		}

		wxMutexGuiEnter();
		wxLogDebug(wxString::Format(wxT("Error processing trace: %s"), e.c_str()));
		wxMutexGuiLeave();
		
		if (t)
		{
			delete t;
			t = NULL;
		}
		return NULL;
	}

#endif // ifdef __APPLE__

	updateProgress(100);

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
