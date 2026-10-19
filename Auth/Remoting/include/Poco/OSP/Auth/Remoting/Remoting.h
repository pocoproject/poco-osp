//
// Remoting.h
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Remoting
//
// Basic definitions for the Poco OSP Auth Remoting library.
// This file must be the first file included by every other OSP Auth Remoting
// header file.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Remoting_Remoting_INCLUDED
#define OSP_Auth_Remoting_Remoting_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSPAuthRemoting_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSPAuthRemoting_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPAuthRemoting_EXPORTS)
		#define OSPAuthRemoting_API __declspec(dllexport)
	#else
		#define OSPAuthRemoting_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPAuthRemoting_API)
	#define OSPAuthRemoting_API
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
