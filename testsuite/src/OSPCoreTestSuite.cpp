//
// OSPCoreTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "OSPCoreTestSuite.h"
#include "VersionTest.h"


CppUnit::Test* OSPCoreTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("OSPCoreTestSuite");

	pSuite->addTest(VersionTest::suite());

	return pSuite;
}
