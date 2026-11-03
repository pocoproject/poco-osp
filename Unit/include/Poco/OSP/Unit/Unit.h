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
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSPUnit_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSPUnit_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPUnit_EXPORTS)
		#define OSPUnit_API __declspec(dllexport)
	#else
		#define OSPUnit_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPUnit_API)
	#if !defined(POCO_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define OSPUnit_API __attribute__ ((visibility ("default")))
	#else
		#define OSPUnit_API
	#endif
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
