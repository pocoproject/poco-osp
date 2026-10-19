//
// AuthTestSuite.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "AuthTestSuite.h"
#include "AuthAdminServiceTest.h"


CppUnit::Test* AuthTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthTestSuite");

	pSuite->addTest(AuthAdminServiceTest::suite());

	return pSuite;
}
