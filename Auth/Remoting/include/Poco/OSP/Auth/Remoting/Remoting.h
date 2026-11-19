//
// Remoting.h
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Remoting
//
// Basic definitions for the POCO OSP Auth Remoting library.
// This file must be the first file included by every other OSP Auth Remoting
// header file.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_Remoting_Remoting_INCLUDED
#define OSP_Auth_Remoting_Remoting_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPAuthRemoting_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPAuthRemoting_EXPORTS)
	#define OSPAuthRemoting_API POCO_API_EXPORT
#else
	#define OSPAuthRemoting_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Auth Remoting library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPAuthRemoting_EXPORTS)
		#pragma comment(lib, "PocoOSPAuthRemoting" POCO_LIB_SUFFIX)
	#endif
#endif


#endif // OSP_Auth_Remoting_Remoting_INCLUDED
