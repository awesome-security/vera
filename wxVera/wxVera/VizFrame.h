#ifndef _VIZFRAME_H_
#define _VIZFRAME_H_

#include "wxvera.h"

#include "Viz.h"
#include "Config.h"
#include "configFrame.h"
#include "traceWizard.h"
#include "threadIdaServer.h"

#define TEXT_SEARCH_DEFAULT ""

class MyApp;
class VeraPane;

// Define a new frame type: this is going to be our main frame
class VizFrame : public wxFrame
{
public:
    // ctor(s)
    VizFrame(const wxString& title, wxPoint pnt, wxSize size, MyApp *parent);
	~VizFrame(void);

    // event handlers (these functions should _not_ be virtual)
    void OnQuit(wxCommandEvent& event);
	void OnOpen(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
	void OnIda(wxCommandEvent& event);
	void OnConfig(wxCommandEvent& event);
	void OnCloseWindow(wxCloseEvent &event);
	void SetProxy(wxString &proxy );
	void mouseMoved(wxMouseEvent& event);
	void leftMouseDown(wxMouseEvent& event);
	void mouseWheelMoved(wxMouseEvent& event);
	void mouseReleased(wxMouseEvent& event);
	void rightMouseDown(wxMouseEvent& event);
	void mouseLeftWindow(wxMouseEvent& event);
	void keyPressed(wxKeyEvent& event);
	void keyReleased(wxKeyEvent& event);
	void render(wxPaintEvent& event);
	void resized(wxSizeEvent& event);
	void CheckForUpdate(wxCommandEvent& event);
	void ProcessEvent(wxCommandEvent & event);
	void SearchTextEvent(wxCommandEvent & event);
	void SearchTextFocused(wxCommandEvent &event);
	bool sendIdaMsg(char *msg);

private:
	VeraPane *				veraPane;
	ConfigFrame *			confFrame;
	wxString				proxy;
	MyApp *					parentApp;
	wxNotebook *			noteBook;
	wxProgressDialog *		dlgProgress;
	threadTraceBuilder *	tbThread;
	wxWizard *				traceWiz;
	traceWizard *			page1;
	threadIdaServer	*		idaServer;
	wxToolBar *				toolBar;
	wxBitmap *bmpOpen;
	wxBitmap *bmpConfig;
	wxBitmap *bmpAbout;
	wxBitmap *bmpIda;
	wxTextCtrl * textSearch;
	bool textSearchIsCleared;
	//wxBitmap *bmpEther;

	void cleanupThreads(void);
	void SetVeraToolbar(wxToolBar *);

    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
	// menu items
	Vera_Quit				= wxID_EXIT,
	// it is important for the id corresponding to the "About" command to have
	// this standard value as otherwise it won't be handled properly under Mac
	// (where it is special and put into the "Apple" menu)
	Vera_About				= wxID_ABOUT,
	Vera_Open				= wxID_OPEN,
	Vera_Config				= wxID_PROPERTIES,
	Vera_ConnectToIDA,
	Vera_ConnectToEther,
	Vera_Check_Updates,
	Vera_Search,
	IDA_SERVER_ID,
};

#endif
