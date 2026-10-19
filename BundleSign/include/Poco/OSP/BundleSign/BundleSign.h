//
// BundleSign.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleSign
//
// Basic definitions for the Poco OSP BundleSign library.
// This file must be the first file included by every other OSP BundleSign
// header file.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_BundleSign_BundleSign_INCLUDED
#define OSP_BundleSign_BundleSign_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSPBundleSign_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSPBundleSign_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPBundleSign_EXPORTS)
		#define OSPBundleSign_API __declspec(dllexport)
	#else
		#define OSPBundleSign_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPBundleSign_API)
	#define OSPBundleSign_API
#endif


//
// Automatically link OSP BundleSign library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPBundleSign_EXPORTS)
		#pragma comment(lib, "PocoOSPBundleSign" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_BundleSign_BundleSign_INCLUDED
