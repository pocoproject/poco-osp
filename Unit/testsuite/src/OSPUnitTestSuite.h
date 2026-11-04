//
// OSPUnitTestSuite.h
//
// Definition of the OSPUnitTestSuite class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSPUnitTestSuite_INCLUDED
#define OSPUnitTestSuite_INCLUDED


#include "CppUnit/TestSuite.h"


class OSPUnitTestSuite
{
public:
	static CppUnit::Test* suite();
};


#endif // OSPUnitTestSuite_INCLUDED
