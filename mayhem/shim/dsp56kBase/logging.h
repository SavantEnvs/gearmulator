#pragma once

// The dsp56300 submodule only supplies the logger to synthLib; this header-only stand-in with the same
// macros lets the fuzz build avoid fetching that submodule. Messages are formatted and dropped.

#include <sstream>
#include <string>
#include <iomanip>

namespace Logging
{
	typedef void (*LogFunc)(const std::string&);

	inline void g_logToConsole(const std::string&) {}
	inline void g_logToFile(const std::string&) {}
	inline void setLogFunc(LogFunc) {}
}

#define LOGTOCONSOLE(ss)	{ Logging::g_logToConsole( (ss).str() ); }
#define LOGTOFILE(ss)		{ Logging::g_logToFile( (ss).str() ); }

#define LOG(S)																												\
do																															\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << __func__ << "@" << __LINE__ << ": " << S;						\
																															\
	LOGTOCONSOLE(__ss__logging_h)																							\
}																															\
while(false)

#define LOGF(S)																												\
do																															\
{																															\
	std::stringstream __ss__logging_h;	__ss__logging_h << S;																\
																															\
	LOGTOFILE(__ss__logging_h)																								\
}																															\
while (false)

#define HEX(S)			std::hex << std::setfill('0') << std::setw(6) << S
#define HEXN(S, n)		std::hex << std::setfill('0') << std::setw(n) << (uint32_t)S
