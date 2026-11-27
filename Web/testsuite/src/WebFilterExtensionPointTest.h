//
// WebFilterExtensionPointTest.h
//
// Definition of the WebFilterExtensionPointTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebFilterExtensionPointTest_INCLUDED
#define WebFilterExtensionPointTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"
#include <memory>


class TemporaryBundle;


class WebFilterExtensionPointTest: public CppUnit::TestCase
{
public:
	WebFilterExtensionPointTest() = delete;
	explicit WebFilterExtensionPointTest(const std::string& name);
	~WebFilterExtensionPointTest() override;

	void setUp() override;
	void tearDown() override;

#if defined(OSPWEB_TEST_LIBRARY)
	void testSecondFilterExtensionForMediaTypeIsRejected();
	void testFilterOfStoppedBundleIsRemoved();
	void testFilterFactoryComesFromItsBundleLibrary();
#endif

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
};


#endif // WebFilterExtensionPointTest_INCLUDED
