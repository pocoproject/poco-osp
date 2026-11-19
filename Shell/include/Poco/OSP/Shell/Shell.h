//
// Shell.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  Shell
//
// Basic definitions for the POCO OSP Shell library.
// This file must be the first file included by every other OSP Shell
// header file.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_Shell_INCLUDED
#define OSP_Shell_Shell_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPShell_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPShell_EXPORTS)
	#define OSPShell_API POCO_API_EXPORT
#else
	#define OSPShell_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Shell library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPShell_EXPORTS)
		#if defined(POCO_DLL)
			#if defined(_DEBUG)
				#pragma comment(lib, "PocoOSPShelld.lib")
			#else
				#pragma comment(lib, "PocoOSPShell.lib")
			#endif
		#endif
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Shell_Shell_INCLUDED
