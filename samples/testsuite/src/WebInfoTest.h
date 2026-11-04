//
// WebInfoTest.h
//
// Definition of the WebInfoTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebInfoTest_INCLUDED
#define WebInfoTest_INCLUDED


#include "CppUnit/TestCase.h"


class WebInfoTest: public CppUnit::TestCase
{
public:
	WebInfoTest() = delete;
	explicit WebInfoTest(const std::string& name);
	~WebInfoTest() override;

	void testServerInformationRequiresLogin();

	static CppUnit::Test* suite();
};


#endif // WebInfoTest_INCLUDED
