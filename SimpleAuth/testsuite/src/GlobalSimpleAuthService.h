//
// GlobalSimpleAuthService.h
//
// Definition of the globalSimpleAuthServiceType() function.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef GlobalSimpleAuthService_INCLUDED
#define GlobalSimpleAuthService_INCLUDED


#include <typeinfo>


[[nodiscard]] const std::type_info& globalSimpleAuthServiceType();
	/// Returns the type of a class named SimpleAuthService in the global
	/// namespace, as another bundle can define it.


#endif // GlobalSimpleAuthService_INCLUDED
