#pragma once

#include <string>

#include "baseLib/logging.h"

// Parsers log every rejected input to stdout; drop it so the fuzzer is not bound by terminal I/O.
namespace
{
	const int g_quiet = (baseLib::logging::setLogFunc([](const std::string&) {}), 0);
}
