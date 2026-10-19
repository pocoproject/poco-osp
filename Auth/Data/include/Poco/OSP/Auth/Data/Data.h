//
// Data.h
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  Auth
//
// Basic definitions for the Poco OSP Auth Data library.
// This file must be the first file included by every other OSP Auth Data
// header file.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Data_Data_INCLUDED
#define OSP_Auth_Data_Data_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSPAuthData_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSPAuthData_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPAuthData_EXPORTS)
		#define OSPAuthData_API __declspec(dllexport)
	#else
		#define OSPAuthData_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPAuthData_API)
	#define OSPAuthData_API
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
