
#include "wxvera.h"
// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
 
// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

// ----------------------------------------------------------------------------
// resources
// ----------------------------------------------------------------------------

//#include "vera.xpm"
//#include "vera-med.xpm"
//#include "vera-small.xpm"

// Local app includes
#include "wxvera.h"
#include "Viz.h"
#include "VizFrame.h"
#include "LegendFrame.h"

#include "wx/iconbndl.h"

#ifdef __WXMAC__
#include <ApplicationServices/ApplicationServices.h>
#endif

BEGIN_EVENT_TABLE(VeraPane, wxGLCanvas)
	EVT_MOTION(VeraPane::mouseMoved)
	EVT_LEFT_DOWN(VeraPane::leftMouseDown)
	EVT_LEFT_UP(VeraPane::mouseReleased)
	EVT_RIGHT_DOWN(VeraPane::rightMouseDown)
	EVT_LEAVE_WINDOW(VeraPane::mouseLeftWindow)
	EVT_SIZE(VeraPane::resized)
	EVT_KEY_DOWN(VeraPane::keyPressed)
	EVT_KEY_UP(VeraPane::keyReleased)
	EVT_CHAR(VeraPane::keyPressed)
	EVT_MOUSEWHEEL(VeraPane::mouseWheelMoved)
	EVT_PAINT(VeraPane::render)
END_EVENT_TABLE()


IMPLEMENT_APP(MyApp)

// ============================================================================
// implementation
// ============================================================================

int MyApp::OnExit()
{
	delete config;
	return wxApp::OnExit();
}

bool MyApp::isNewVersionAvailable(void)
{
	// Retrieve the URL from the update site and check if we need a new version

	bool ret = false;
	wxString urlString(wxT(__VERA_UPDATE_URL__ "?myversion=" __VERA_VERSION__));

	wxURL url(urlString);
	url.SetProxy(config->proxyInfo);

	//	url.SetURL(urlString);

	if (url.GetError() == wxURL_NOERR)
	{
		wxString htmlData;
		wxInputStream *in = url.GetInputStream();

		if (in && in->IsOk())
		{
			wxStringOutputStream html_stream(&htmlData);
			in->Read(html_stream);

			// Check the length (should be less than 5 characters)
			if (htmlData.Length() <= 5)
			{
				wxString version(wxT(__VERA_VERSION__));

				double myVersion			= -1.0;
				double availableVersion			= -1.0;

				// Versions are expressed as a simple double value
				version.ToDouble(&myVersion);
				htmlData.ToDouble(&availableVersion); 

				// Check for version converted successfully
				if (availableVersion != -1.0 && myVersion != -1.0) 
				{
					if (myVersion == availableVersion)
						ret = false;
					// Only return true if our version is less than the
					// current version
					else if (myVersion < availableVersion) 
						ret = true;
					else
						ret = false;
				}
				
			}

		}
	
		if (in)
		{
			delete in;
			in = NULL;
		}
	}

	return ret;
}

// For some reason my event code wouldn't see the key down presses, so I do it here.
// This is a kludge
int MyApp::FilterEvent(wxEvent &event)
{
	if (event.GetEventType() == wxEVT_KEY_DOWN)
	{
		if (vizFrame != NULL)
		{
			vizFrame->keyPressed((wxKeyEvent &)event);
			return -1;
		}
	}
	return -1;	
}

// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
    // call the base class initialization method, currently it only parses a
    // few common command-line options but it could be do more in the future
    if ( !wxApp::OnInit() )
        return false;

	config = new Config();

    // create the main application window
    vizFrame = new VizFrame(wxT("VERA - Version " __VERA_VERSION__),
			    config->startPoint,
			    config->windowSize,
			    this);

    if (vizFrame == NULL)
    {
	    wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"),
					__FILE__,
					__LINE__));
	    return false;
    }

    // Set the proxy address
    if (config->proxyInfo.Length() > 0)
	    vizFrame->SetProxy(config->proxyInfo);
    
    // Add the icons so they show up nicely in the taskbar
    wxIconBundle ib;
    ib.AddIcon(wxIcon(vera_xpm));
    ib.AddIcon(wxIcon(vera_med_xpm));
    ib.AddIcon(wxIcon(vera_small_xpm));
    vizFrame->SetIcons(ib);

    // and show it (the frames, unlike simple controls, are not shown when
    // created initially)
    vizFrame->Show(true);
    
    // Check for the latest version from the website
    wxCommandEvent evt;
    evt.SetInt(VERA_QUIET_UPDATE_CHECK);
    vizFrame->CheckForUpdate(evt);
    
#ifdef __WXMAC__
    // Special processing needed to bring the GUI to the front for the Mac.
    // http://wiki.wxwidgets.org/WxMac_Issues
    ProcessSerialNumber PSN;
    GetCurrentProcess(&PSN);
    TransformProcessType(&PSN, kProcessTransformToForegroundApplication);
#endif
	
    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned false here, the
    // application would exit immediately.
    return true;
}

