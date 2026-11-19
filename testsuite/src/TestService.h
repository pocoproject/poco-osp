//
// TestService.h
//
// Definition of the TestService class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestService_INCLUDED
#define TestService_INCLUDED


#include "Poco/OSP/Service.h"
#include "Poco/AutoPtr.h"
#include <typeinfo>


class TestService: public Poco::OSP::Service
	/// A service without behaviour, for registering in the tests.
{
public:
	using Ptr = Poco::AutoPtr<TestService>;

	TestService() = default;

	[[nodiscard]] const std::type_info& type() const override;
		/// Returns the type information of TestService.

	[[nodiscard]] bool isA(const std::type_info& otherType) const override;
		/// Returns true iff otherType is TestService or Service.

protected:
	~TestService() override = default;
};


#endif // TestService_INCLUDED
