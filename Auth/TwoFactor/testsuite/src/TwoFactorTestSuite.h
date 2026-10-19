//
// TwoFactorTestSuite.h
//
// Definition of the WebTestSuite class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef TwoFactorTestSuite_INCLUDED
#define TwoFactorTestSuite_INCLUDED


#include "CppUnit/TestSuite.h"


class TwoFactorTestSuite
{
public:
	static CppUnit::Test* suite();
};


#endif // TwoFactorTestSuite_INCLUDED
