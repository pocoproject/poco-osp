//
// Web.h
//
// Library: OSP/Web
// Package: Web
// Module:  Web
//
// Basic definitions for the POCO OSP Web library.
// This file must be the first file included by every other OSP Web
// header file.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_Web_INCLUDED
#define OSP_Web_Web_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPWeb_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPWeb_EXPORTS)
	#define OSPWeb_API POCO_API_EXPORT
#else
	#define OSPWeb_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Web library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPWeb_EXPORTS)
		#pragma comment(lib, "PocoOSPWeb" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Web_Web_INCLUDED
