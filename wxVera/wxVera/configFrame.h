#ifndef _CONFIG_FRAME_H_
#define _CONFIG_FRAME_H_

#include "wxvera.h"

enum
{
	BUTTON_Save = wxID_HIGHEST + 5,
	BUTTON_Cancel
};

class ConfigFrame :
	public wxFrame
{
public:
	ConfigFrame(Config *veraConfig, wxFrame *parent);
	~ConfigFrame(void);

	Config *		veraConfig;

	void cancelButton(wxCommandEvent &event);
	void saveButton(wxCommandEvent &event);

private:

	wxBoxSizer *	configMainSizer;
	

	// These are sizers for all the configuration options
	// Proxy info is the most important 
	wxBoxSizer *	proxySizer;
	wxBoxSizer *	updateSizer;
	wxTextCtrl *	proxyInfo;
	wxBoxSizer *    buttonSizer;
	wxCheckBox *	updateCheckBox;

	wxFrame *myParent;

	DECLARE_EVENT_TABLE()
};

#endif
