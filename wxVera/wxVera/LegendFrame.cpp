#include "wxvera.h"

LegendFrame::LegendFrame(const wxString& title, wxPoint pnt, wxSize size, MyApp *parent)
       : wxFrame(NULL, wxID_ANY, title, pnt, size)
{
	parentApp = parent;
}

LegendFrame::~LegendFrame(void)
{
}
