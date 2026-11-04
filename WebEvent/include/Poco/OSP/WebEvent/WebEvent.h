//
// WebEvent.h
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEvent
//
// Basic definitions for the Poco OSP WebEvent library.
// This file must be the first file included by every other OSP WebEvent
// header file.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_WebEvent_WebEvent_INCLUDED
#define OSP_WebEvent_WebEvent_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the WebEvent_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// WebEvent_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPWebEvent_EXPORTS)
		#define OSPWebEvent_API __declspec(dllexport)
	#else
		#define OSPWebEvent_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPWebEvent_API)
	#if !defined(POCO_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define OSPWebEvent_API __attribute__ ((visibility ("default")))
	#else
		#define OSPWebEvent_API
	#endif
#endif


//
// Automatically link OSP WebEvent library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPWebEvent_EXPORTS)
		#pragma comment(lib, "PocoOSPWebEvent" POCO_LIB_SUFFIX)
	#endif
#endif


// Turn off some annoying warnings
#if defined(_MSC_VER)
	#pragma warning(disable:4503) // decorated name length exceeded, name was truncated
#endif


#endif // OSP_WebEvent_WebEvent_INCLUDED
