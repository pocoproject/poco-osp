//
// Data.h
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  Auth
//
// Basic definitions for the POCO OSP Auth Data library.
// This file must be the first file included by every other OSP Auth Data
// header file.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_Data_Data_INCLUDED
#define OSP_Auth_Data_Data_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPAuthData_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPAuthData_EXPORTS)
	#define OSPAuthData_API POCO_API_EXPORT
#else
	#define OSPAuthData_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Auth library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPAuthData_EXPORTS)
		#pragma comment(lib, "PocoOSPAuthData" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Auth_Data_Data_INCLUDED
