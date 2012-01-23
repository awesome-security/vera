#include "wxvera.h"

#include "wx/thread.h"
#include "VizFrame.h"

#ifdef _USE_WX_EVENTS_
BEGIN_EVENT_TABLE(VizFrame, wxFrame)
    EVT_MENU(Vera_Quit,  VizFrame::OnQuit)
	EVT_MENU(Vera_Open, VizFrame::OnOpen)
    EVT_MENU(Vera_About, VizFrame::OnAbout)
	EVT_MENU(Vera_Help, VizFrame::OnHelp)
	EVT_CLOSE(VizFrame::OnCloseWindow)
	EVT_MENU(Vera_Check_Updates, VizFrame::CheckForUpdate)
#ifdef _WIN32
        EVT_MENU(Vera_ConnectToIDA, VizFrame::OnIda)
#endif
	EVT_MENU(Vera_Config, VizFrame::OnConfig)
	EVT_MOUSEWHEEL(VizFrame::mouseWheelMoved)
	EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, VizFrame::ProcessEvent)
    EVT_TEXT(Vera_Search, VizFrame::SearchTextFocused)
	EVT_TEXT_ENTER(Vera_Search, VizFrame::SearchTextEvent)
	EVT_MENU(Vera_Search, VizFrame::SearchTextButton)
	EVT_MENU(Vera_Home, VizFrame::HomeDisplay)
	EVT_MENU(Vera_Rewind, VizFrame::RewindTemporalTrace)
	EVT_MENU(Vera_Play, VizFrame::PlayTemporalTrace)
	EVT_MENU(Vera_FastForward, VizFrame::FastForwardTemporalTrace)
	EVT_MENU(Vera_StopTemporal, VizFrame::StopTemporalTrace)
	EVT_TIMER(Vera_AnimationTimer, VizFrame::AnimationTimer)
END_EVENT_TABLE()
#endif

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

// frame constructor
VizFrame::VizFrame(const wxString& title, wxPoint pnt, wxSize size, MyApp *parent)
       : wxFrame(NULL, wxID_ANY, title, pnt, size)
{
	page1		= NULL;
	traceWiz	= NULL;
	parentApp	= parent;
	tbThread    = NULL;
	#ifdef _WIN32
	idaServer   = NULL;
	#endif
	textSearchIsCleared = false;
	animationTimer = NULL;

	// This is for the "tabbed" view at the top of the application
	noteBook = new wxNotebook(this, wxID_ANY, pnt, size, wxNB_TOP);

	if (noteBook == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	// Start with the veraPane null and just display a text label
	veraPane = NULL;

	wxRichTextCtrl *introMessage = new wxRichTextCtrl(
		noteBook, 
		wxID_ANY, 
		wxEmptyString, 
		wxDefaultPosition, 
		wxSize(200, 200),
		wxNO_BORDER|wxWANTS_CHARS|wxRE_READONLY|wxRE_CENTER_CARET);

	introMessage->BeginAlignment(wxTEXT_ALIGNMENT_CENTER);
	introMessage->BeginBold();
	introMessage->BeginFontSize(50);
	introMessage->Newline();
	introMessage->Newline();
	introMessage->WriteText(wxT("Welcome to VERA v" __VERA_VERSION__));
	introMessage->EndFontSize();
	introMessage->EndBold();
	introMessage->Newline();
	introMessage->EndAlignment();
	
	// First page to be displayed.
	noteBook->AddPage(introMessage, wxT("Visualization"));

	// Create the File Menu
	wxMenu *fileMenu = new wxMenu;

	if (fileMenu == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	// File Menu Items
	fileMenu->Append(Vera_Open, wxT("&Open\tCtrl-O"), wxT("Open a GML file"));
	fileMenu->Append(Vera_Quit, wxT("E&xit\tAlt-X"), wxT("Quit this program"));

	// Create the tools menu
	wxMenu *toolsMenu = new wxMenu;

	if (toolsMenu == NULL)
	{ 
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

#ifdef _WIN32
	toolsMenu->Append(Vera_ConnectToIDA, wxT("Start &IDA Server\tCtrl-I"), wxT("Start IDA Pro connection"));
#endif
	toolsMenu->Append(wxID_PROPERTIES, wxT("O&ptions"), wxT("Open Options and Configuration"));

	// Create the Help Menu
	wxMenu *helpMenu = new wxMenu;

	if (helpMenu == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	// Help Menu Items
	helpMenu->Append(Vera_Help, wxT("&Help\tF1"), wxT("Show help document"));
	helpMenu->Append(Vera_About, wxT("&About..."), wxT("Show about dialog"));
	helpMenu->Append(Vera_Check_Updates, wxT("&Updates"), wxT("Check for updates"));

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar();

	if (menuBar == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	menuBar->Append(fileMenu, wxT("&File"));
	menuBar->Append(toolsMenu, wxT("&Tools"));
	menuBar->Append(helpMenu, wxT("&Help"));

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	// Create the Tool Bar
	// Open, IDA, Configuration, Search for Address

	toolBar = new wxToolBar(this, wxID_ANY);

	if (NULL == toolBar)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	// Icons for the toolBar
	bmpOpen = new wxBitmap(folder_32_xpm);
	bmpConfig = new wxBitmap(gear_32_xpm);
	bmpHelp = new wxBitmap(help_32_xpm);
	bmpFind = new wxBitmap(find_xpm);
	bmpHome = new wxBitmap(home_xpm);
	bmpRewind = new wxBitmap(rewind_xpm);
	bmpPlay = new wxBitmap(play_xpm);
	bmpFastForward = new wxBitmap(fastforward_xpm);
	bmpStop = new wxBitmap(stop_xpm);
#ifdef _WIN32
	// IDA integration is only working in Windows at the moment
	bmpIda = new wxBitmap(ida_32_xpm);
#endif
	//bmpEther = new wxBitmap(ether_32_xpm);

	textSearch = NULL; // This is initialized in SetVeraToolbar to give the search box the right toolbar base

	// Call the code to add the toolbar
	SetVeraToolbar(toolBar);

	CreateStatusBar(2);
	SetStatusText(wxT("Ready"));

}

void VizFrame::SetVeraToolbar(wxToolBar *tb)
{
	// Add the icons, and reuse events where able
	tb->AddTool(wxID_OPEN, *bmpOpen, wxT("Open GML graph or trace file"));
#ifdef _WIN32
	tb->AddTool(Vera_ConnectToIDA, *bmpIda, wxT("Start IDA Pro module listener"));
#endif
	tb->AddTool(wxID_PROPERTIES, *bmpConfig, wxT("Configure VERA"));
	tb->AddTool(Vera_Help, *bmpHelp, wxT("About VERA"));
	tb->AddTool(Vera_Home, *bmpHome, wxT("Reset the view back to the default"));


	// Actual controls have to have the new toolbar as the base, so set that up here.
	if (NULL != textSearch)
		delete textSearch;
	
	textSearch = new wxTextCtrl(tb, Vera_Search, wxT(TEXT_SEARCH_DEFAULT), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

	if (NULL == textSearch)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}
	
	tb->AddControl(textSearch);
	tb->AddTool(Vera_Search, *bmpFind, wxT("Search for address"));

	tb->AddTool(Vera_Rewind, *bmpRewind, wxT("Rewind temporal trace view to the beginning of execution"));
	tb->AddTool(Vera_Play, *bmpPlay, wxT("Play (animate) the temporal trace view"));
	tb->AddTool(Vera_FastForward, *bmpFastForward, wxT("Fast forward to the end of the temporal trace view"));
	tb->AddTool(Vera_StopTemporal, *bmpStop, wxT("Stop the temporal trace view"));
	tb->Realize();

	SetToolBar(tb);
}

VizFrame::~VizFrame(void)
{
	cleanupThreads();

	// Clean up all the allocated icons
	delete bmpOpen;
	delete bmpConfig;
	delete bmpHelp;
	delete bmpFind;
	delete bmpHome;
	delete bmpStop;
	delete bmpFastForward;
	delete bmpPlay;
	delete bmpRewind;

#ifdef _WIN32
	delete bmpIda;
	bmpIda     = NULL;
#endif
	bmpOpen    = NULL;
	bmpConfig  = NULL;
	bmpHelp    = NULL;
	bmpFind    = NULL;
	bmpHome    = NULL;
	bmpStop	   = NULL;
	bmpFastForward = NULL;
	bmpPlay    = NULL;
	bmpRewind  = NULL;

	if (animationTimer)
	{
		delete animationTimer;
		animationTimer = NULL;
	}
}

// event handlers

void VizFrame::cleanupThreads(void)
{
	#ifdef _WIN32
	if(idaServer)
	{
		idaServer->Delete();
		idaServer = NULL;
	}
	#endif

}

void VizFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{

	cleanupThreads();

	// true is to force the frame to close
	Close(true);
}

inline void VizFrame::StartVeraPane(void)
{
	if (veraPane == NULL)
	{
		int args[] = VERA_OBJECT_ARGS;
		veraPane = new VeraPane(noteBook, args, this);

		if (veraPane == NULL)
		{
			wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
			return;
		}

		noteBook->DeleteAllPages();
		noteBook->AddPage(veraPane, wxT("Visualization"));
	}
}

void VizFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
	// Open a dialog for just the .gml files
	wxFileDialog OpenDialog(
		this, 
		wxT("Choose a file to open"),
		wxEmptyString,
		wxEmptyString,
		wxT("GML files (*.gml;*.trace)|*.gml;*.trace"),
		wxFD_OPEN, 
		wxDefaultPosition);

	

	// If the user clicks the ok box, then present the image
	if (OpenDialog.ShowModal() == wxID_OK)
	{
		wxString path = OpenDialog.GetPath();

		/*
		  There are multiple ways of opening files. First a .trace file can be used to generate
		  the graph files. Second a .gml extension can be used to open the graphs directly.

		  The trace files have two different supported types. First, traces from the Ether system.
		  Second, traces from a veratrace (Intel PIN) based system.
		  
		 */

		if (path.EndsWith(wxT(".gml")))
		{
			// Do the visualization. Check the return value here.
			StartVeraPane();
			if (!veraPane->openFile(path))
			{
				wxMessageBox(
					wxString::Format(
						wxT("Error loading GML file %s, invalid format. Is this actually a .gml file?"),
						path.c_str())
					);
				this->SetTitle(wxString::Format(wxT("%s"), wxT(__VERA_WINDOW_TITLE__)));
				this->SetStatusText(wxString::Format(wxT("Error loading %s"), path.c_str()));
				return;
			}
			else
			{
				this->SetStatusText(wxString::Format(wxT("Loaded %s"), path.c_str()));
			}
		}
		else if (path.EndsWith(wxT(".trace")))
		{
			// If the file doesn't exist update title bar, status bar, display an error box
			if (!wxFileExists(path))
			{
				wxMessageBox(
					wxString::Format(
						wxT("Error loading trace file %s"),
						path.c_str())
					);
				this->SetTitle(wxString::Format(wxT("%s"), wxT(__VERA_WINDOW_TITLE__)));
				this->SetStatusText(wxString::Format(wxT("Error loading %s"), path.c_str()));
				return;
			}

			// We need more information to open a trace file, so use that to open the file.
			traceWiz = new wxWizard(this, 
						wxID_ANY, 
						wxT("Process trace files"), 
						wxNullBitmap, 
						wxDefaultPosition, 
						wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			
			if (traceWiz == NULL)
			{
				wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"),
							    __FILE__, __LINE__));
				return;
			}

			page1 = new traceWizard(traceWiz);

			if (page1 == NULL)
			{
				wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"),
							    __FILE__, __LINE__));
				return;
			}

			traceWiz->GetPageAreaSizer()->Add(page1);

			if (traceWiz->RunWizard(page1))
			{
				// Make sure the check boxes are actually selected
				if (! (page1->m_genBblAddressesCheckBox->GetValue() ||
				       page1->m_genAllAddressesCheckBox->GetValue()) )
				{
					wxMessageBox(wxT("You must select at least one\n"
							 "address processing mode"),
						     wxT("Trace Processing Error"), 
						     wxICON_ERROR);
					return;

				}

				wxString saveFile = page1->m_saveFile->GetValue();

				// Check for invalid save file or duplicates
				if ( saveFile.StartsWith(wxT(NO_GML_FILE_SELECTED_TEXT)) )
				{
					wxMessageBox(wxT("You must choose where to save\nthe graph data file."),
						     wxT("Trace Processing Error"), 
						     wxICON_ERROR);
					return;
				}

				// Check for invalid exe files
				int answer = -1;
				bool doProcessExe = true;

				if ( page1->m_origExeFile->GetValue().StartsWith(wxT(NO_EXE_FILE_SELECTED_TEXT)) )
				{
					// Ask the user if they want to generate a trace without the executable
					answer = wxMessageBox(
							wxT("Do you want to generate a trace without an executable?\n"
								"Certain features will be unavailable if one is not provided."),
							wxT("Missing .exe file"),
							wxCENTER | wxYES_NO);
	
					switch (answer)
					{
					case wxYES:
						// Nothing else is needed to do
						doProcessExe = false;
						break;
					case wxNO:
						// Return with nothing being done
						return;
						break;
					default:
						wxLogDebug(wxT("Strange error"));
						break;
					}
				}

				// Check to see if the GML file already exists
				bool doProcessBasicBlocks = page1->m_genBblAddressesCheckBox->GetValue();
				bool doProcessAllBlocks   = page1->m_genAllAddressesCheckBox->GetValue();
				
				// Check for the basic blocks file
				if (doProcessBasicBlocks == true)
				{
					wxString fullFileName = prependFileName(saveFile, wxT("bbl-"));
					if (wxFileExists(fullFileName) )
					{
						int answer = wxMessageBox(
							wxString::Format(wxT("File %s already exists, do you want to overwrite it?"), 
									 fullFileName.c_str()),
							wxT("File already exists"),
							wxCENTER | wxYES_NO );
						
						switch (answer)
						{
						case wxNO:
							doProcessBasicBlocks = false;
							break;
						case wxYES:
							doProcessBasicBlocks = true;
							break;
						default:
							// Weirdness
							wxLogDebug(wxString::Format(wxT("Strange case reached at line %u in %s"),  __LINE__, __FILE__ ));
							return;
						}
					}
				}

				if ( doProcessAllBlocks == true)
				{
					wxString fullFileName = prependFileName(saveFile, wxT("all-"));
					if (wxFileExists(fullFileName) )
					{
						int answer = wxMessageBox(
							wxString::Format(wxT("File %s already exists, do you want to overwrite it?"), 
									 fullFileName.c_str()),
							wxT("File already exists"),
							wxCENTER | wxYES_NO );
						
						switch (answer)
						{
						case wxNO:
							doProcessAllBlocks = false;
							break;
						case wxYES:
							doProcessAllBlocks = true;
							break;
						default:
							// Weirdness
							wxLogDebug(wxString::Format(wxT("Strange case reached at line %u in %s"),  __LINE__, __FILE__ ));
							return;
						}
					}
				}

				if (doProcessAllBlocks == false && doProcessBasicBlocks == false)
				{
					this->SetStatusText(wxT("No files loaded"));
					return;
				}

				// Begin the actual processing
				dlgProgress = new wxProgressDialog(wxT("Processing trace file"),
								   wxT("Processing the tracefile"),
								   100,
								   this);
				
				if (dlgProgress == NULL)
				{
					wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"),
								    __FILE__, __LINE__));
					return;
				}

				dlgProgress->Show(true);

				// Create a new thread to handle the actual trace generation
				tbThread = new threadTraceBuilder(path, 
								  (doProcessExe ? page1->m_origExeFile->GetValue() : wxString(wxT("")) ), // Empty string indicates processing trace file without a PE
								  saveFile, 
								  doProcessBasicBlocks,
								  doProcessAllBlocks,
								  page1->m_layoutAlgorithmComboBox->GetSelection(), 
								  this,
								  dlgProgress);

				if (tbThread == NULL)
				{
					wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
					return;
				}

				wxThreadError err = tbThread->Create();

				if (wxTHREAD_NO_ERROR != err)
				{
					wxLogDebug(wxT("Error creating the trace builder thread %d"), err);
					return;
				}

				// Actually run the thread
				err = tbThread->Run();

				if (wxTHREAD_NO_ERROR != err)
				{
					wxLogDebug(wxT("Error running the trace builder thread %d"), err);
					return;
				}

			}
			
			this->SetStatusText(wxString::Format(wxT("Processing %s..."), path.c_str()));
			traceWiz->Destroy();

		}
		else
		{
			wxMessageBox(
				wxString::Format(
					wxT("Error loading file %s. File needs to have .gml or .trace as the file extension."),
					path.c_str())
				);
			this->SetTitle(wxString::Format(wxT("%s"), wxT(__VERA_WINDOW_TITLE__)));
			this->SetStatusText(wxString::Format(wxT("Error loading %s"), path.c_str()));
			return;
		}
		
		this->SetTitle(wxString::Format(wxT("%s - %s"), wxT(__VERA_WINDOW_TITLE__), path.c_str()));
		this->SetStatusText(wxString::Format(wxT("Loaded %s"), path.c_str()));

	}

	// Clean up the dialog
	OpenDialog.Destroy();

	// Trigger a render event
	if (veraPane)
		veraPane->DrawAndRender();
}

// This function strikes me as ugly and non-optimal -Danny
void VizFrame::ProcessEvent(wxCommandEvent & event)
{
	int			traceType		= event.GetInt();
	wxString	filename;

	// Only print out the trace message for the trace messages
	switch(traceType)
	{
	case THREAD_TRACE_NONE_PROCESSED: // There was an error with one of the traces
		wxLogDebug(wxT("No trace processed"));
		return;
	case THREAD_TRACE_ERROR:
		{
			wxString errorMsg = event.GetString();
			wxLogDebug(wxT("Error processing file: %s"), errorMsg.c_str());
			this->dlgProgress->Destroy();
			this->SetTitle(wxString::Format(wxT("%s"), wxT(__VERA_WINDOW_TITLE__)));
			wxMessageBox(wxString::Format(wxT("Error processing trace file: %s"), errorMsg.c_str()),
						 wxT("Graph Processing Error"),
						 wxICON_ERROR);

			return;
		}
	case THREAD_TRACE_BASIC_BLOCKS_PROCESSED: // Processed a basic block graph
	case THREAD_TRACE_BOTH_PROCESSED: // Processed both a basic block and an instruction graph
	case THREAD_TRACE_ALL_ADDRESSES_PROCESSED: // All instruction graph processed
		wxLogDebug(wxT("Trying to open %s %d"), event.GetString(), event.GetInt());
		break;
	default:
		break;
	}
	
	
	// Right now try to open the all-%s file. Later open both into different notebook tabs
	switch (traceType)
	{
	case THREAD_TRACE_BASIC_BLOCKS_PROCESSED:		// Open the basic block if it's the only one available
		filename = prependFileName(event.GetString(), wxT("bbl-"));
		break;
	case THREAD_TRACE_BOTH_PROCESSED:				// Prefer the all-%s.gml file over the basic block version
	case THREAD_TRACE_ALL_ADDRESSES_PROCESSED:
		filename = prependFileName(event.GetString(), wxT("all-"));
		break;
	case THREAD_IDA_FINISHED_NOERROR:
		wxLogDebug(wxT("IDA Thread finished without errors"));
		break;
	case THREAD_IDA_FINISHED_ERROR:
		wxLogDebug(wxT("IDA Thread finished with error"));
		break;
	case THREAD_IDA_MSG_RECEIVED:
		wxLogDebug(wxT("Received a message from IDA: (%s)"), event.GetString());
		
		if (strcmp(event.GetString().ToAscii(), "OK  \n") == 0)
			wxLogDebug(wxT("Navigate succeeded"));
		else
			wxLogDebug(wxT("Navigate Failed"));
		
		break;
	default:
		wxLogDebug(wxT("Strange trace state received"));
		return;
		break;
	}

	// Open the file if it's a trace processing event
	switch(traceType)
	{
	case THREAD_TRACE_BASIC_BLOCKS_PROCESSED:
	case THREAD_TRACE_BOTH_PROCESSED:
	case THREAD_TRACE_ALL_ADDRESSES_PROCESSED:
		StartVeraPane();
		veraPane->openFile(filename);
		this->SetStatusText(wxString::Format(wxT("Loaded %s"), filename.c_str()));
		break;
	default:
		break;
	}
	
}
void VizFrame::OnHelp(wxCommandEvent& WXUNUSED(event))
{
	wxMessageBox(wxT("VERA Quick Help"
		"Move the view:	left-click with your mouse on any part of the screen and drag\n"
		"Zoom: Scroll-wheel or A/Z keys\n"
		"Open: CTRL-O\n"
		"Help: F1\n"
		"Connect to IDA: CTRL-I\n"
		"Navigate in IDA: Right-click address (not a library) with the mouse\n"
		)
		);
}

void VizFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	// Tech Transfer's contribution to the product.
	wxMessageBox(wxT("VERA version " __VERA_VERSION__ " built " __TIMESTAMP__ "\n\n")
				 wxT("Copyright (C) 2011 Los Alamos National Laboratory, All Rights Reserved\n")
				 wxT("Patent Pending, LA-CC-10-131\n\n")
				 wxT("This program was prepared by Los Alamos National Security, LLC at Los ")
				 wxT("Alamos National Laboratory (LANL) under contract No. DE-AC52-06NA25396 ")
				 wxT("with the U.S. Department of Energy (DOE). All rights in the program are ")
				 wxT("reserved by the DOE and Los Alamos National Security, LLC. The U.S. ")
				 wxT("Government retains ownership of all rights in the program and copyright ")
				 wxT("subsisting therein. All rights not granted below are reserved.\n\n")
				 wxT("This program may be used for noncommercial, nonexclusive purposes for ")
				 wxT("internal research, development and evaluation and demonstration purposes ")
				 wxT("only. The right to reproduce, distribute, display publicly, prepare ")
				 wxT("derivative works or compilations thereof is prohibited.\n\n")
				 wxT("NEITHER THE UNITED STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, ")
				 wxT("NOR THE LOS ALAMOS NATIONAL SECURITY, LLC, NOR ANY OF THEIR EMPLOYEES, ")
				 wxT("MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL LIABILITY ")
				 wxT("OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR USEFULNESS OF ANY ")
				 wxT("INFORMATION, APPARATUS, PRODUCT, OR PROCESS DISCLOSED, OR REPRESENTS ")
				 wxT("THAT ITS USEWOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.\n\n")
				 wxT("Email dquist@offensivecomputing.net for more information\n\n")
				 wxT("Portions of this software are copyright (C) 2009\n")
				 wxT("The FreeType Project (www.freetype.org), All Rights Reserved"),
                 wxT("About VERA"),
                 wxOK | wxICON_INFORMATION,
                 this);
}

void VizFrame::OnConfig(wxCommandEvent &event)
{
	// Create a new configuration frame for VERA
	confFrame = new ConfigFrame(parentApp->config, this);
	confFrame->Show(true);
}

// The text search box gets focused. 
void VizFrame::SearchTextFocused(wxCommandEvent &event)
{
	if (!textSearchIsCleared && event.GetString().Len() != 0)
	{
		textSearch->Clear();
		textSearchIsCleared = true;
	}
}

void VizFrame::SearchTextButton(wxCommandEvent &event)
{
	wxCommandEvent ev;
	ev.SetString(textSearch->GetValue());

	this->SearchTextEvent(ev);
}

void VizFrame::SearchTextEvent(wxCommandEvent &event)
{
	if (veraPane == NULL)
	{
		wxMessageBox(wxT("No graph is loaded, try again"),
					 wxT("Invalid Search"),
					 wxICON_ERROR);
		return;
	}

	if (event.GetString().Find(wxT(TEXT_SEARCH_DEFAULT)) != wxNOT_FOUND)
	{
		wxMessageBox(wxT("Please search for an address or function name"),
					 wxT("Invalid Search"),
					 wxICON_ERROR);
		return;
	}

	std::string searchVal = event.GetString().MakeLower().ToAscii();
	node_t *s = NULL;

	if (strcmp(searchVal.c_str(), "start") == 0)
		s = veraPane->searchByString(START_NODE_LABEL);
	else
		s = veraPane->searchByString(searchVal);

	if (s != NULL)
	{
		veraPane->goToPoint(s->x, s->y, MAX_ZOOM);
		this->SetStatusText(wxString::Format(wxT("Found %s"), s->label));
	}
	else
	{
		this->SetStatusText(wxString::Format(wxT("Could not find %s"), searchVal.c_str()));
		wxLogMessage(wxT("Could not find search term %s"), searchVal.c_str());
	}


}

void VizFrame::CheckForUpdate(wxCommandEvent& event)
{
	if (parentApp->config->checkForNewVersion == true && parentApp->isNewVersionAvailable())
	{
		int ret = wxMessageBox(wxT("A new version of VERA is available, \n"
					   "would you like to go download it?"), 
				       wxT("New version is available"), 
				       wxYES_NO);
		
		if (ret == wxYES)
		{
			// Open VERA URL
			wxLaunchDefaultBrowser(wxT(__VERA_UPDATE_DOWNLOAD_URL__));
		}
		else if (ret == wxNO)
		{
			ret = wxMessageBox(wxT("Would you like to check for updates on startup?"),
					   wxT("Check for updates"), wxYES_NO);

			if (ret == wxNO)
			{
				parentApp->config->checkForNewVersion = false;
				parentApp->config->writeConfig(this);
			}
		}
	}
	else if (event.GetInt() != VERA_QUIET_UPDATE_CHECK)
	{
		wxMessageBox(wxT("You are at the latest version of VERA"), wxT("Up-to-date"), wxOK);
	}
}

void VizFrame::HomeDisplay(wxCommandEvent &event)
{
	if (veraPane)
		veraPane->resetView();
}

void VizFrame::AnimationTimer(wxTimerEvent &event)
{
	wxLogDebug(wxT("I am an animation timer!"));

	if (veraPane->doAnimationStep(1) == VIZ_ANIMATION_FINISHED)
		if (animationTimer)
			animationTimer->Stop();

	veraPane->DrawAndRender();
}

void VizFrame::RewindTemporalTrace(wxCommandEvent &event)
{
	wxLogDebug(wxT("Rewind"));
	
	if (animationTimer)
	{
		animationTimer->Stop();
		veraPane->setAnimationStep(0);
		veraPane->DrawAndRender();
	}
}

void VizFrame::PlayTemporalTrace(wxCommandEvent &event)
{
	wxLogDebug(wxT("Play"));

	if (animationTimer == NULL)
	{
		animationTimer = new wxTimer(this, Vera_AnimationTimer);

		if (animationTimer == NULL)
		{
			wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
			return;
		}

		veraPane->setAnimationStatus(true);
		animationTimer->Start(animationTimerInterval);
	}
	else
	{
		animationTimer->Start();
	}
}

void VizFrame::FastForwardTemporalTrace(wxCommandEvent &event)
{
	wxLogDebug(wxT("Fast Forward"));

	if (animationTimer)
	{
		animationTimer->Stop();
		veraPane->setAnimationStep(-1);
		veraPane->DrawAndRender();
	}
}

void VizFrame::StopTemporalTrace(wxCommandEvent &event)
{
	wxLogDebug(wxT("Stop"));

	if (animationTimer)
	{
		animationTimer->Stop();
	}
}

void VizFrame::OnCloseWindow(wxCloseEvent &event)
{
	// Set the configuration info
	parentApp->config->writeConfig(this);

	wxFrame::OnCloseWindow(event);
}

void VizFrame::SetProxy(wxString &p)
{
	if (p.Length() > 0)
	{
		this->proxy = p;
	}
}

// Event handlers that pass to the underlying Notebook pages

void VizFrame::mouseMoved(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->mouseMoved(event);
	}

}

void VizFrame::leftMouseDown(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->leftMouseDown(event);
	}

}

void VizFrame::mouseWheelMoved(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->mouseWheelMoved(event);
	}

}

void VizFrame::mouseReleased(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->mouseReleased(event);
	}
}

void VizFrame::rightMouseDown(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->rightMouseDown(event);
	}
}

void VizFrame::mouseLeftWindow(wxMouseEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->mouseLeftWindow(event);
	}
}

void VizFrame::keyPressed(wxKeyEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( event.GetEventObject()->IsKindOf(CLASSINFO(VeraPane)) && curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->keyPressed(event);
	}
}

void VizFrame::keyReleased(wxKeyEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->keyReleased(event);
	}
}

void VizFrame::render(wxPaintEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->render(event);
	}
}

void VizFrame::resized(wxSizeEvent& event)
{
	wxWindow *curWin = noteBook->GetCurrentPage();

	// If the name is a GLCanvas, this means we have a VeraPane object we can send the event to.
	if ( curWin->GetName().Contains(wxT("GLCanvas")) )
	{
		((VeraPane *) curWin)->resized(event);
	}
}

void VizFrame::OnIda(wxCommandEvent& event)
{
// IDA Integration only works on Windows
#ifdef _WIN32
	wxToolBar *newToolBar = new wxToolBar(this, wxID_ANY);
	wxBitmap * tmp = bmpIda;

	// Disabled to get IDA ready to run code
	if (this->idaServer == NULL)
	{
		idaServer = new threadIdaServer(this);
		idaServer->Create();
		idaServer->Run();

		bmpIda = new wxBitmap(idaConnected_32_xpm);
	}
	else
	{
		idaServer->Delete();
		idaServer = NULL;

		bmpIda = new wxBitmap(ida_32_xpm);
	}
	
	delete tmp;

	// Draw the new toolbar with the new buttons
	this->SetVeraToolbar(newToolBar);
	newToolBar->Realize();

	// Clear the memory of the old one
	delete toolBar;
	toolBar = newToolBar;
#else
	wxLogDebug(wxT("Not implemented!"));
#endif
}

 // Stub, needs to be finished
bool VizFrame::sendIdaMsg(char *msg)
{
#ifdef _WIN32
	if (idaServer != NULL)
		return idaServer->sendData(msg, strlen(msg));
	else
		return false;
#else
	return false;
#endif
}
