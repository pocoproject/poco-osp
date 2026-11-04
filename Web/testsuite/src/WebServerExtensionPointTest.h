//
// WebServerExtensionPointTest.h
//
// Definition of the WebServerExtensionPointTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebServerExtensionPointTest_INCLUDED
#define WebServerExtensionPointTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"
#include <memory>


class TemporaryBundle;


class WebServerExtensionPointTest: public CppUnit::TestCase
{
public:
	WebServerExtensionPointTest() = delete;
	explicit WebServerExtensionPointTest(const std::string& name);
	~WebServerExtensionPointTest() override;

	void setUp() override;
	void tearDown() override;

	void testCommonAttributes();
	void testCommonAttributeDefaults();
	void testInvalidAllowSpecialization();
	void testInvalidResponseFormat();
#if defined(OSPWEB_TEST_LIBRARY)
	void testRequestHandlerFactoryComesFromItsBundleLibrary();
#endif

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
};


#endif // WebServerExtensionPointTest_INCLUDED
