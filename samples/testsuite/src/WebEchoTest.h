//
// WebEchoTest.h
//
// Definition of the WebEchoTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebEchoTest_INCLUDED
#define WebEchoTest_INCLUDED


#include "CppUnit/TestCase.h"


class WebEchoTest: public CppUnit::TestCase
{
public:
	WebEchoTest() = delete;
	explicit WebEchoTest(const std::string& name);
	~WebEchoTest() override;

	void testRequestValuesAreEscaped();
	void testMalformedEscapeIsBadRequest();
	void testTooManyFormFieldsIsBadRequest();
	void testMalformedMultipartFormIsBadRequest();
	void testCookieAndCredentialValuesAreNotShown();
	void testRequestWithoutHostIsAnswered();

	static CppUnit::Test* suite();
};


#endif // WebEchoTest_INCLUDED
