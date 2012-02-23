// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define COMPONENTNAME "Queue Contents Editor"
#define COMPONENTVERSION "0.5.1"
#define COMPONENTCONFIGVERSION 4

// Changelog in config versions
/*
v4:
- unsigned long m_border introduced in ui_element_settings
*/

#define TITLEFORMAT_WIKIPAGEURL "http://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Titleformat_Reference"
#define COMPONENT_WIKIPAGEURL "http://wiki.hydrogenaudio.org/index.php?title=Foobar2000:Components/Queue_Contents_Editor_(foo_queuecontents)"
#define FORUMURL "http://www.hydrogenaudio.org/forums/index.php?showtopic=73648"

#include "targetver.h"

#include "atlcrack.h"
#include "../ATLHelpers/ATLHelpers.h"
#include "../SDK/foobar2000.h"
#include "../helpers/helpers.h"
//#include "../SDK/component.h"
//#include "atlframe.h" //WTL::CDialogResize
//#include "atlctrlx.h" //WTL::CWaitCursor

#include "resource.h"
#include "guids.h"

#ifndef _DEBUG 

#include <ostream>
#include <cstdio>
#include <ios>

struct debug_nullstream : std::ostream {
	struct nullbuf: std::streambuf {
	int overflow(int c) { return traits_type::not_eof(c); }
	} m_sbuf;
	debug_nullstream() : std::ios(&m_sbuf), std::ostream(&m_sbuf) {}
};

#define DEBUG_PRINT debug_nullstream()
#endif

#ifdef _DEBUG
#define DEBUG_PRINT console::formatter() << COMPONENTNAME ": " 
#endif