//
// TwoFactor.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  TwoFactor
//
// Basic definitions for the POCO OSP Auth TwoFactor library.
// This file must be the first file included by every other OSP Auth TwoFactor
// header file.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_TwoFactor_TwoFactor_INCLUDED
#define OSP_Auth_TwoFactor_TwoFactor_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPAuthTwoFactor_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPAuthTwoFactor_EXPORTS)
	#define OSPAuthTwoFactor_API POCO_API_EXPORT
#else
	#define OSPAuthTwoFactor_API POCO_API_IMPORT
#endif


//
// Automatically link OSP Auth library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPAuthTwoFactor_EXPORTS)
		#pragma comment(lib, "PocoOSPAuthTwoFactor" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Auth_TwoFactor_TwoFactor_INCLUDED
