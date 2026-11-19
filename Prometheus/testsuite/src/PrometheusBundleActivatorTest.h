//
// PrometheusBundleActivatorTest.h
//
// Definition of the PrometheusBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef PrometheusBundleActivatorTest_INCLUDED
#define PrometheusBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class PrometheusBundleActivatorTest: public CppUnit::TestCase
{
public:
	PrometheusBundleActivatorTest() = delete;
	explicit PrometheusBundleActivatorTest(const std::string& name);
	~PrometheusBundleActivatorTest() override;

	void testKeepAliveTimeoutIsInSeconds();
	void testFailedStartCanBeRepeated();
	void testDispatcherCanRegisterAgain();
	void testStopUnregistersCollectors();
	void testMetricsNeedConfiguredPermission();
	void testMetricsAcceptOtherNameOfAccount();
	void testMetricsWithoutPermission();
	void testMissingAuthServiceIsLoggedOnce();
	void testThreadPoolMetricsHaveOneHeader();
	void testThreadPoolOfUnregisteredDispatcherIsNotExported();
	void testLogIntervalOutsideRangeIsRejected();
	void testLogIntervalOfOneSecondLogsMetrics();
	void testLongestLogIntervalIsAccepted();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // PrometheusBundleActivatorTest_INCLUDED
