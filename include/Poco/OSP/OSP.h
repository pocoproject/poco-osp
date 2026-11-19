//
// OSP.h
//
// Library: OSP
// Package: Core
// Module:  OSP
//
// Basic definitions for the POCO OSP library.
// This file must be the first file included by every other OSP
// header file.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_OSP_INCLUDED
#define OSP_OSP_INCLUDED


#include "Poco/Foundation.h"


//
// OSP_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSP_EXPORTS)
	#define OSP_API POCO_API_EXPORT
#else
	#define OSP_API POCO_API_IMPORT
#endif


//
// Automatically link OSP library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSP_EXPORTS)
		#pragma comment(lib, "PocoOSP" POCO_LIB_SUFFIX)
	#endif
#endif


#if defined(POCO_NO_SHAREDLIBS)
#define POCO_OSP_STATIC
#endif


#endif // OSP_OSP_INCLUDED
