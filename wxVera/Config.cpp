#include "wxvera.h"

#include "Config.h"

void Config::parseConfig(void)
{

	isConfigParsed = false;

	wxTextFile configFile(configPath);
	
	if ( configFile.Exists() )
	{
		configFile.Open(configPath);

		// Check to see if the file opened
		if ( ! configFile.IsOpened() )
		{
			wxLogDebug(wxT("Could not open config file (%s)"), wxT("Config Parse Error"));
			return;
		}
	}
	else
	{
		wxLogDebug(wxT("Config file does not exist, this could be the first time it was opened"));
		return;
	}

	// The file should be opened and ready to read

	for(wxString str = configFile.GetFirstLine() ;  !configFile.Eof() ; str = configFile.GetNextLine())
	{
		if ( str.StartsWith(wxT("gui_start_point=")) )
		{
			int x = 0;
			int y = 0;

			const char *line = str.ToAscii();
			sscanf(line, "%*[^=]=%i,%i", &x, &y);


			startPoint.x = (int) x;
			startPoint.y = (int) y;

		}
		else if ( str.StartsWith(wxT("gui_window_size=")) )
		{
			int x = 0;
			int y = 0;

			const char *line = str.ToAscii();
			sscanf(line, "%*[^=]=%i,%i", &x, &y);
		
			windowSize.x = (int) x;
			windowSize.y = (int) y;
		}
		else if ( str.StartsWith(wxT("proxy=")) )
		{
			char in[256] = {0};
			const char *line = str.ToAscii();

			// Make sure there's actually data here.
			if (line != NULL && strlen(line) > strlen("proxy=") && strlen(line) < sizeof(in) - 1)
			{
				sscanf(line, "%*[^=]=%[^\n]", in);
				this->proxyInfo = wxString(in, wxConvUTF8);
			}
		}
		else if ( str.StartsWith(wxT("checkforupdates=")))
		{
			char in[256] = {0};
			const char *line = str.ToAscii();

			if (strlen(line) > strlen("checkforupdates=") && strlen(line) < sizeof(in) - 1)
			{
				sscanf(line, "%*[^=]=%s", in);
				if (in[0] == 'f')
					checkForNewVersion = false;
				else if (in[0] == 't')
					checkForNewVersion = true;
			}
		}
	}

	configFile.Close();

}

Config::Config(wxString configFile)
{
	// Set some default values for the starting point and window size
	startPoint = wxPoint(10,10);
	windowSize = wxSize(800,600);
	checkForNewVersion = true;

	this->configPath = configFile;
	parseConfig();
}

Config::Config(void)
{
	// Set some default values for the starting point and window size
	startPoint = wxPoint(10,10);
	windowSize = wxSize(800,600);
	checkForNewVersion = true;

	wxString configDir = wxStandardPaths::Get().GetUserDataDir();

	// If the directory doesn't exist, create it.
	if (! ::wxDirExists(configDir) )
	{
		if ( wxMkdir(configDir) )
		{
			wxLogDebug(wxT("Created config directory"));
		}
		else
		{
			wxMessageBox(wxString::Format(wxT("Could not create directory (%s)"), configDir.c_str()), wxT("Config Parse Error"));
			return;
		}
	}

	// Set string to ~/vera.ini
	configPath.append(configDir);
	configPath.append(wxFileName::GetPathSeparator());
	configPath.append(wxT("vera.ini"));

	parseConfig();
}

Config::~Config(void)
{
}

void Config::writeConfig(wxFrame *frame)
{
	if (frame == NULL)
	{
		wxLogDebug(wxT("No frame provided"));
		return;
	}

	wxString configDir = wxStandardPaths::Get().GetUserDataDir();

	// If the directory doesn't exist, create it.
	if (! ::wxDirExists(configDir) )
	{
		if ( wxMkdir(configDir) )
		{
			wxLogDebug(wxT("Created config directory"));
		}
		else
		{
			wxMessageBox(wxString::Format(wxT("Could not create directory (%s)"), configDir.c_str()), wxString(wxT("Config Parse Error")));
			return;
		}
	}

	// Set string to ~/.vera/vera.ini
	wxString configPath;
	configPath.append(configDir);
	configPath.append(wxFileName::GetPathSeparator());
	configPath.append(wxT("vera.ini"));

	//wxString configPath = wxString::Format(wxT("%s%svera.ini"), configDir.get, wxFileName::GetPathSeparator());	

	wxTextFile configFile(configPath);
	
	if (! configFile.Exists() )
	{
		configFile.Create();
	}

	configFile.Open(configPath);

	// Check to see if the file opened
	if ( ! configFile.IsOpened() )
	{
		wxLogDebug(wxT("Could not open config file"), wxT("Config Parse Error"));
		return;
	}

	wxPoint pos		= frame->GetPosition();
	wxSize s		= frame->GetClientSize();

	configFile.Clear();
	configFile.AddLine(wxT("# Auto-generated configuration file"));
	configFile.AddLine(wxT("version=" __VERA_VERSION__));
	configFile.AddLine(wxString::Format(wxT("gui_start_point=%i,%i"), pos.x, pos.y) );
	configFile.AddLine(wxString::Format(wxT("gui_window_size=%i,%i"), s.x, s.y) );
	configFile.AddLine(wxString::Format(wxT("proxy=%s"), proxyInfo.c_str()));
	configFile.AddLine(wxString::Format(wxT("checkforupdates=%s"), 
		checkForNewVersion == true ? wxT("true") : wxT("false")));

	configFile.Write();
	configFile.Close();

}
