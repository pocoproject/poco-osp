//
// WebEvent.h
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEvent
//
// Basic definitions for the POCO OSP WebEvent library.
// This file must be the first file included by every other OSP WebEvent
// header file.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_WebEvent_WebEvent_INCLUDED
#define OSP_WebEvent_WebEvent_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPWebEvent_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPWebEvent_EXPORTS)
	#define OSPWebEvent_API POCO_API_EXPORT
#else
	#define OSPWebEvent_API POCO_API_IMPORT
#endif


//
// Automatically link OSP WebEvent library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPWebEvent_EXPORTS)
		#pragma comment(lib, "PocoOSPWebEvent" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_WebEvent_WebEvent_INCLUDED
