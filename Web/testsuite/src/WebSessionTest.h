//
// WebSessionTest.h
//
// Definition of the WebSessionTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebSessionTest_INCLUDED
#define WebSessionTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"
#include <memory>


class TemporaryBundle;


class WebSessionTest: public CppUnit::TestCase
{
public:
	WebSessionTest() = delete;
	explicit WebSessionTest(const std::string& name);
	~WebSessionTest() override;

	void setUp() override;
	void tearDown() override;

	void testAttributesReturnsCopy();
	void testTimeoutUpdatesExpiration();
	void testClearKeepsCSRFToken();
	void testClearKeepsCSRFTokenInStore();
	void testCSRFTokenCheckNeedsSessionToken();
	void testSessionOutlivesBundleLoader();
	void testVersionReadWhileAttributesChange();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
};


#endif // WebSessionTest_INCLUDED
