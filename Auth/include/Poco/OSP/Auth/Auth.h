//
// Auth.h
//
// Library: OSP/Auth
// Package: Auth
// Module:  Auth
//
// Basic definitions for the POCO OSP Auth library.
// This file must be the first file included by every other OSP Auth
// header file.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_Auth_INCLUDED
#define OSP_Auth_Auth_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPAuth_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPAuth_EXPORTS)
	#define OSPAuth_API POCO_API_EXPORT
#else
	#define OSPAuth_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Auth library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPAuth_EXPORTS)
		#pragma comment(lib, "PocoOSPAuth" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Auth_Auth_INCLUDED
