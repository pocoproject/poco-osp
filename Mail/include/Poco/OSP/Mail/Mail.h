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
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Mail_Mail_INCLUDED
#define OSP_Mail_Mail_INCLUDED


#include "Poco/OSP/OSP.h"


//
// OSPMail_EXPORTS is defined only while this library is compiled (see POCO_API_EXPORT in Poco/Foundation.h).
//
#if defined(OSPMail_EXPORTS)
	#define OSPMail_API POCO_API_EXPORT
#else
	#define OSPMail_API POCO_API_IMPORT
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
