//
// Driver.cpp
//
// Console-based test driver for Poco WebServer.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "CppUnit/TestRunner.h"
#include "WebTestSuite.h"


#if defined(__APPLE__) && defined(__has_feature)
#if __has_feature(address_sanitizer)
extern "C" const char* __asan_default_options()
	// AddressSanitizer on macOS keeps the globals of an unloaded library registered and
	// poisoned, so a library loaded later at the same address reports false ODR and
	// global-buffer-overflow errors.
{
	return "detect_odr_violation=0:report_globals=0";
}
#endif
#endif


CppUnitMain(WebTestSuite)
