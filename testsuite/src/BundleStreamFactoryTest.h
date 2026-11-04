//
// BundleStreamFactoryTest.h
//
// Definition of the BundleStreamFactoryTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef BundleStreamFactoryTest_INCLUDED
#define BundleStreamFactoryTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class BundleStreamFactoryTest: public CppUnit::TestCase
{
public:
	BundleStreamFactoryTest(const std::string& name);
	~BundleStreamFactoryTest() override;

	void testOpenReadsBundleResource();
	void testOpenOfMissingResourceOrBundleThrowsNotFound();
	void testUriWithoutPathIsRejected();
	void testDefaultOpenerUsesRegisteredFactory();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // BundleStreamFactoryTest_INCLUDED
