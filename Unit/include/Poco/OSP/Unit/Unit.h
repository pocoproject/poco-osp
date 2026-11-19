//
// Unit.h
//
// Library: OSP/Unit
// Package: Unit
// Module:  Unit
//
// Basic definitions for the POCO OSP Unit library.
// This file must be the first file included by every other OSP Unit
// header file.
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Unit_Unit_INCLUDED
#define OSP_Unit_Unit_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPUnit_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPUnit_EXPORTS)
	#define OSPUnit_API POCO_API_EXPORT
#else
	#define OSPUnit_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Unit library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPUnit_EXPORTS)
		#pragma comment(lib, "PocoOSPUnit" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Unit_Unit_INCLUDED
