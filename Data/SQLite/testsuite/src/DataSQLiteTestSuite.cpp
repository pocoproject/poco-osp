//
// DataSQLiteTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DataSQLiteTestSuite.h"
#include "SQLiteBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* DataSQLiteTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("DataSQLiteTestSuite");

	pSuite->addTest(SQLiteBundleActivatorTest::suite());

	return pSuite;
}
