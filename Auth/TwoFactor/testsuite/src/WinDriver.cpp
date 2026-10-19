//
// WinDriver.cpp
//
// Windows test driver for Poco OSP Auth TwoFactor.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "WinTestRunner/WinTestRunner.h"
#include "TwoFactorTestSuite.h"


class TestDriver: public CppUnit::WinTestRunnerApp
{
	void TestMain()
	{
		CppUnit::WinTestRunner runner;
		runner.addTest(TwoFactorTestSuite::suite());
		runner.run();
	}
};


TestDriver theDriver;
