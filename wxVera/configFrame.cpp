#include "wxvera.h"

// This wrapper is to keep Editors from blotting stuff out.
#ifdef _USE_WX_EVENTS_
BEGIN_EVENT_TABLE(ConfigFrame, wxFrame)
	EVT_BUTTON(BUTTON_Save, ConfigFrame::saveButton)
	EVT_BUTTON(BUTTON_Cancel, ConfigFrame::cancelButton)
END_EVENT_TABLE()
#endif

ConfigFrame::ConfigFrame(Config *veraConfig, wxFrame *parent) 
	: wxFrame(parent, wxID_ANY, wxT("VERA Configuration"))
{ 
	this->veraConfig = veraConfig;
	configMainSizer			= new wxBoxSizer(wxVERTICAL);
	proxySizer				= new wxBoxSizer(wxHORIZONTAL);
	updateSizer				= new wxBoxSizer(wxHORIZONTAL);
	buttonSizer				= new wxBoxSizer(wxHORIZONTAL);

	myParent = parent;

	if (configMainSizer == NULL || proxySizer == NULL || buttonSizer == NULL || updateSizer == NULL)
	{ 
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	this->SetBackgroundColour(wxColor(wxT("white")));

	configMainSizer->Add(new wxStaticText(this, wxID_ANY,
		wxT("Configuration options. Most of the things that you might\n"
			"need to adjust are listed here, but please refer to the\n"
			"documentation for other options.")),
			0,
			wxALL,
			5);

	proxyInfo = new wxTextCtrl(this, wxID_ANY, veraConfig->proxyInfo, wxDefaultPosition, wxSize(200, 20));

	if (proxyInfo == NULL)
	{ 
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	proxySizer->Add(new wxStaticText(this, wxID_ANY, wxT("Proxy (host:port)")), 0, wxALL, 5);
	proxySizer->Add(proxyInfo);
	proxySizer->Fit(this);

	configMainSizer->Add(proxySizer, 0, wxALL, 5);

	// Set up check for updates Sizer
	updateCheckBox = new wxCheckBox(this, wxID_ANY, wxT("Check for new versions on startup?"));

	if (updateCheckBox == NULL)
	{ 
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	updateCheckBox->SetValue(veraConfig->checkForNewVersion);

	updateSizer->Add(updateCheckBox);
	updateSizer->Fit(this);

	configMainSizer->Add(updateSizer, 0, wxALL, 5);

	wxButton * saveButton = new wxButton(this, BUTTON_Save, wxT("&Save"));
	wxButton * cancelButton = new wxButton(this, BUTTON_Cancel, wxT("&Cancel"));

	if (saveButton == NULL || cancelButton == NULL)
	{ 
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	buttonSizer->Add(saveButton);
	buttonSizer->Add(cancelButton);
	buttonSizer->Fit(this);

	configMainSizer->Add(buttonSizer);

	SetSizer(configMainSizer);
	configMainSizer->Fit(this);
	
}

void ConfigFrame::cancelButton(wxCommandEvent &event)
{
	this->Close();
}

void ConfigFrame::saveButton(wxCommandEvent &event)
{
	// Copy the data out of the right places.
	veraConfig->proxyInfo = proxyInfo->GetValue();
	veraConfig->checkForNewVersion = updateCheckBox->GetValue();
	veraConfig->writeConfig(myParent);

	this->Close();
}

ConfigFrame::~ConfigFrame(void)
{
	
}
