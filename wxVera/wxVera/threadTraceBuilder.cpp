#include "wxvera.h"

threadTraceBuilder::threadTraceBuilder(wxString traceFile, 
				       wxString exeFile, 
				       wxString gmlSaveFile, 
				       bool doBbl, 
				       bool doAll,
				       wxFrame *parentFrame,
				       wxProgressDialog *prog)
{
	m_traceFile = traceFile;
	m_exeFile = exeFile;
	m_gmlSaveFile = gmlSaveFile;
	m_prog = prog;
	m_doBbl = doBbl;
	m_doAll = doAll;
	m_parentFrame = parentFrame;

}

void *threadTraceBuilder::Entry()
{
	try 
	{
		// Process the basic blocks
		if (m_doBbl)
		{
			// Add "bbl-" to the beginning of the file name
			wxString outfilename = prependFileName(m_gmlSaveFile, wxT("bbl-"));
			wxString tmpfilename = prependFileName(m_gmlSaveFile, wxT("tmp-bbl-"));
			
			Trace *t = NULL;
			try 
			{
				t = new Trace(m_traceFile.GetFullPath(), m_exeFile.GetFullPath(), outfilename);
			}
			catch (char *e)
			{
				wxMutexGuiEnter();
				wxLogDebug(wxString::Format(wxT("Error: %s"), e));
				wxMutexGuiLeave();
			}

			if (t == NULL)
			{
				//wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
				return NULL;
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

			if (m_prog)
			{
				wxMutexGuiEnter();
				m_prog->Update(30);
				wxMutexGuiLeave();
			}

			delete t;
			
			if (!wxRemoveFile(tmpfilename))
			{
				//wxLogDebug(wxT("Could not remove temp file"));
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

			Trace *t = new Trace(m_traceFile.GetFullPath(), m_exeFile.GetFullPath(), outfilename);
			
			if (t == NULL)
			{
				wxMutexGuiEnter();
				wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
				wxMutexGuiLeave();
				return NULL;
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
	}


	if (m_prog)
	{
		wxMutexGuiEnter();
		m_prog->Update(100);
		wxMutexGuiLeave();
	}

	if (m_prog != NULL)
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
