#include "wxvera.h"

BEGIN_EVENT_TABLE(traceWizard, wxWizardPage)
	EVT_BUTTON(BUTTON_Exe, traceWizard::exeLoad)
	EVT_BUTTON(BUTTON_Gml, traceWizard::gmlSave)
END_EVENT_TABLE()

traceWizard::traceWizard(wxWizard *parent, wxWizardPage *prev, wxWizardPage *next) : wxWizardPage(parent)
{
	exeFileSet = gmlFileSet = false;

	m_next = next;
	m_prev = prev;

	mainSizer					= new wxBoxSizer(wxVERTICAL);
	traceSizer					= new wxBoxSizer(wxHORIZONTAL);
	gmlSizer					= new wxBoxSizer(wxHORIZONTAL);
	m_genAllAddressesCheckBox	= new wxCheckBox(this, wxID_ANY, wxT("All addresses"));
	m_genBblAddressesCheckBox	= new wxCheckBox(this, wxID_ANY, wxT("Basic blocks"));

	wxArrayString choices;
	choices.Add(wxT("OGDF Layout"));
	choices.Add(wxT("iGraph Layout"));

	m_layoutAlgorithmComboBox	= new wxComboBox(this, 
		wxID_ANY, 
		wxT("Choose a layout algorithm"),
		wxDefaultPosition, 
		wxDefaultSize, 
		choices,
		wxCB_READONLY | wxCB_DROPDOWN);

	m_genAllAddressesCheckBox->SetValue(true);
	m_genBblAddressesCheckBox->SetValue(true);

	mainSizer->Add(new wxStaticText(this, wxID_ANY, 
		wxT("You have chosen to open a trace file. In order to process it\n"
		    "correctly you will need to provide the original executable,\n"
			"as well as save a copy of the resulting graph file. Please\n"
			"specify the files in the next two boxes.")), 
			0, 
			wxALL, 
			5);

	// Trace file save
	mainSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Original executable file:")), 0, wxALL, 5);

	m_origExeFile = new wxTextCtrl(this, wxID_ANY, wxT(NO_EXE_FILE_SELECTED_TEXT), wxDefaultPosition, wxSize(200, 20));

	if (m_origExeFile == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	m_traceButton = new wxButton(this, BUTTON_Exe, wxT("..."));
	
	if (m_traceButton == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	traceSizer->Add(m_origExeFile);
	traceSizer->Add(m_traceButton);
	traceSizer->Fit(this);

	mainSizer->Add(traceSizer, 0, wxALL, 5);

	// GML file save
	mainSizer->Add(new wxStaticText(this, wxID_ANY, wxT("GML Save file as:")), 0, wxALL, 5);

	m_saveFile = new wxTextCtrl(this, wxID_ANY, wxT(NO_GML_FILE_SELECTED_TEXT), wxDefaultPosition, wxSize(200, 20));

	if (m_saveFile == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	m_gmlButton = new wxButton(this, BUTTON_Gml, wxT("..."));

	if (m_gmlButton == NULL)
	{
		wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}
	
	gmlSizer->Add(m_saveFile);
	gmlSizer->Add(m_gmlButton);
	gmlSizer->Fit(this);

	mainSizer->Add(gmlSizer, 0, wxALL, 5);

	//// Options for all address and basic blocks
	mainSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Generate graphs for:")), 0, wxALL, 5);
	
	mainSizer->Add(m_genAllAddressesCheckBox);
	mainSizer->Add(m_genBblAddressesCheckBox);
	mainSizer->Add(m_layoutAlgorithmComboBox);

	SetSizer(mainSizer);
	mainSizer->Fit(this);
}

wxWizardPage *traceWizard::GetNext() const
{
	return m_next;
}

wxWizardPage *traceWizard::GetPrev() const
{
	return m_prev;
}

void traceWizard::SetNext(wxWizardPage *page)
{
	this->m_next = page;
}

void traceWizard::SetPrev(wxWizardPage *page)
{
	this->m_prev = page;
}

void traceWizard::exeLoad(wxCommandEvent &WXUNUSED(event))
{
	wxFileDialog OpenDialog(this,
		wxT("Choose an executable Windows PE file to open"),
		wxEmptyString,
		wxEmptyString,
		wxT("EXE files (*.exe;*.sys;*.dll;*.scr;*.ocx)|*.exe;*.sys;*.dll*.scr;*.ocx"),
		wxFD_OPEN,
		wxDefaultPosition);

	if ( OpenDialog.ShowModal() == wxID_OK )
	{
		wxString path = OpenDialog.GetPath();
		m_origExeFile->ChangeValue(path);
	}

	OpenDialog.Destroy();
}

void traceWizard::gmlSave(wxCommandEvent &WXUNUSED(event))
{
	wxFileDialog OpenDialog(this,
		wxT("Choose an GML file to save to"),
		wxEmptyString,
		wxEmptyString,
		wxT("GML files (*.gml)|*.gml"),
		wxFD_SAVE,
		wxDefaultPosition);

	if ( OpenDialog.ShowModal() == wxID_OK )
	{
		wxString path = OpenDialog.GetPath();
		m_saveFile->ChangeValue(path);
	}

	OpenDialog.Destroy();
}
