//
// WebSessionSampleTest.h
//
// Definition of the WebSessionSampleTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebSessionSampleTest_INCLUDED
#define WebSessionSampleTest_INCLUDED


#include "CppUnit/TestCase.h"


class WebSessionSampleTest: public CppUnit::TestCase
{
public:
	WebSessionSampleTest() = delete;
	explicit WebSessionSampleTest(const std::string& name);
	~WebSessionSampleTest() override;

	void testGetDoesNotChangeSession();
	void testPostWithoutTokenIsRejected();
	void testPostWithTokenSetsAndRemovesEntry();
	void testServerAttributesCannotBeSet();
	void testNumberOfEntriesIsLimited();
	void testConcurrentRequestsInOneSession();
	void testConcurrentPostsKeepNumberOfEntriesLimited();
	void testStoppingSampleReleasesEntries();

	static CppUnit::Test* suite();
};


#endif // WebSessionSampleTest_INCLUDED
