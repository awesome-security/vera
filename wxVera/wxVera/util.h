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
bool isHexString(const char *str, size_t len);

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define TOUPPER(s, l) for (size_t i = 0 ; i < (l) ; i++) (s)[i] = toupper((s)[i])
#define TOLOWER(s, l) for (size_t i = 0 ; i < (l) ; i++) (s)[i] = tolower((s)[i])
