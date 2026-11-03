//
// OSP.h
//
// Library: OSP
// Package: Core
// Module:  OSP
//
// Basic definitions for the POCO OSP library.
// This file must be the first file included by every other OSP
// header file.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_OSP_INCLUDED
#define OSP_OSP_INCLUDED


#include "Poco/Foundation.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSP_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSP_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSP_EXPORTS)
		#define OSP_API __declspec(dllexport)
	#else
		#define OSP_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSP_API)
	#if !defined(POCO_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define OSP_API __attribute__ ((visibility ("default")))
	#else
		#define OSP_API
	#endif
#endif


//
// Automatically link OSP library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSP_EXPORTS)
		#pragma comment(lib, "PocoOSP" POCO_LIB_SUFFIX)
	#endif
#endif


#if defined(POCO_NO_SHAREDLIBS)
#define POCO_OSP_STATIC
#endif


#endif // OSP_OSP_INCLUDED
