//
// WebIndexTest.h
//
// Definition of the WebIndexTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebIndexTest_INCLUDED
#define WebIndexTest_INCLUDED


#include "CppUnit/TestCase.h"


class WebIndexTest: public CppUnit::TestCase
{
public:
	WebIndexTest() = delete;
	explicit WebIndexTest(const std::string& name);
	~WebIndexTest() override;

	void testIndexIsServedForRequestWithQuery();

	static CppUnit::Test* suite();
};


#endif // WebIndexTest_INCLUDED
