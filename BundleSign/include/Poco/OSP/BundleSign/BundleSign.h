//
// BundleSign.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleSign
//
// Basic definitions for the POCO OSP BundleSign library.
// This file must be the first file included by every other OSP BundleSign
// header file.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_BundleSign_BundleSign_INCLUDED
#define OSP_BundleSign_BundleSign_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPBundleSign_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPBundleSign_EXPORTS)
	#define OSPBundleSign_API POCO_API_EXPORT
#else
	#define OSPBundleSign_API POCO_API_IMPORT
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
