//
// GreetingServiceTest.h
//
// Definition of the GreetingServiceTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef GreetingServiceTest_INCLUDED
#define GreetingServiceTest_INCLUDED


#include "CppUnit/TestCase.h"


class GreetingServiceTest: public CppUnit::TestCase
{
public:
	GreetingServiceTest() = delete;
	explicit GreetingServiceTest(const std::string& name);
	~GreetingServiceTest() override;

	void testGreetingIsInTheConfiguredLanguage();

	static CppUnit::Test* suite();

private:
	[[nodiscard]] std::string greeting(const std::string& language);
};


#endif // GreetingServiceTest_INCLUDED
