#ifndef __TRACE_WIZARD_H__
#define __TRACE_WIZARD_H__

#include "wxvera.h"

enum
{
	BUTTON_Exe = wxID_HIGHEST + 1,
	BUTTON_Gml
};

class traceWizard :
	public wxWizardPage
{
public:
	traceWizard(wxWizard *parent, 
			    wxWizardPage *prev=NULL, 
				wxWizardPage *next=NULL);

	virtual wxWizardPage *		GetPrev() const;
	virtual wxWizardPage *		GetNext() const;
	void						SetPrev(wxWizardPage *);
	void						SetNext(wxWizardPage *);
	void						exeLoad(wxCommandEvent &event);
	void						gmlSave(wxCommandEvent &event);

	wxBoxSizer *				mainSizer;
	wxBoxSizer *				traceSizer;
	wxBoxSizer *				gmlSizer;
	wxTextCtrl *				m_origExeFile;
	wxTextCtrl *				m_saveFile;
	wxButton *					m_traceButton;
	wxButton *					m_gmlButton;
	wxCheckBox *				m_genAllAddressesCheckBox;
	wxCheckBox *				m_genBblAddressesCheckBox;
	wxComboBox *				m_layoutAlgorithmComboBox;

	DECLARE_EVENT_TABLE();

private:
	wxWizardPage *				m_prev;
	wxWizardPage *				m_next;
	wxEvtHandler *				parentFrame;
	bool						exeFileSet;
	bool						gmlFileSet;

};


#endif

