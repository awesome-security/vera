/**
 ** Copyright (C) 2009 Danny Quist. All rights reserved.
 **/

#pragma once

#ifndef _WXVERA_H_

#include "wx/string.h"
#include "wx/filename.h"

#ifdef _WIN32
#include <windows.h>
#endif

#else

#include "wxvera.h"

#endif

int		xtoi(const char* xs, unsigned int* result);
wxString	prependFileName(wxFileName input, wxString stringToPrepend);

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
