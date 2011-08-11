#ifndef __LEGEND_FRAME__
#define __LEGEND_FRAME__

#include "wxvera.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
 
// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/sizer.h"
#include "wx/glcanvas.h"
#include "wx/stdpaths.h"
#include "wx/filefn.h"
#include "wx/filename.h"
#include "wx/file.h"
#include "wx/textfile.h"
#include "wx/url.h"
#include "wx/sstream.h"
#include "wx/utils.h"


class MyApp;

class LegendFrame : public wxFrame
{
public:
	LegendFrame(const wxString& title, wxPoint pnt, wxSize size, MyApp *parent);
	~LegendFrame(void);

private:
	MyApp *parentApp;
};

#endif
