#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "wxvera.h"

class Config
{
public:
	
	Config(wxString configFile);
	Config();
	~Config();
	void writeConfig(wxFrame *frame);
	wxPoint		startPoint;
	wxSize		windowSize;
	wxString	proxyInfo;
	bool		isConfigParsed;
	bool		checkForNewVersion;

private:
	void parseConfig(void);
	wxString configPath;

};


#endif

