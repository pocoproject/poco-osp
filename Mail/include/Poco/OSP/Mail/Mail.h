//
// Mail.h
//
// Library: OSP/Mail
// Package: Mail
// Module:  Mail
//
// Basic definitions for the POCO OSP Mail library.
// This file must be the first file included by every other OSP Mail
// header file.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Mail_Mail_INCLUDED
#define OSP_Mail_Mail_INCLUDED


#include "Poco/OSP/OSP.h"


//
// The following block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the OSPMail_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file sees
// OSPMail_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
//
#if defined(_WIN32) && defined(POCO_DLL)
	#if defined(OSPMail_EXPORTS)
		#define OSPMail_API __declspec(dllexport)
	#else
		#define OSPMail_API __declspec(dllimport)
	#endif
#endif


#if !defined(OSPMail_API)
	#if !defined(POCO_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define OSPMail_API __attribute__ ((visibility ("default")))
	#else
		#define OSPMail_API
	#endif
#endif


//
// Automatically link OSP Mail library.
//
#if defined(_MSC_VER)
	#if !defined(POCO_NO_AUTOMATIC_LIBS) && !defined(OSPMail_EXPORTS)
		#pragma comment(lib, "PocoOSPMail" POCO_LIB_SUFFIX)
	#endif
#endif


#endif // OSP_Mail_Mail_INCLUDED
