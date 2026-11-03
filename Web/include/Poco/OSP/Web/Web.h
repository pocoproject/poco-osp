//
// Web.h
//
// Library: OSP/Web
// Package: Web
// Module:  Web
//
// Basic definitions for the POCO OSP Web library.
// This file must be the first file included by every other OSP Web
// header file.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_Web_INCLUDED
#define OSP_Web_Web_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the WebServer_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// WebServer_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPWeb_EXPORTS)
		#define OSPWeb_API __declspec(dllexport)
	#else
		#define OSPWeb_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPWeb_API)
	#if !defined(POCO_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define OSPWeb_API __attribute__ ((visibility ("default")))
	#else
		#define OSPWeb_API
	#endif
#endif


//
// Automatically link OSP Web library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPWeb_EXPORTS)
		#pragma comment(lib, "PocoOSPWeb" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_Web_Web_INCLUDED
