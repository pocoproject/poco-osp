//
// TCPBundleActivatorTest.h
//
// Definition of the TCPBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TCPBundleActivatorTest_INCLUDED
#define TCPBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class TCPBundleActivatorTest: public CppUnit::TestCase
{
public:
	TCPBundleActivatorTest() = delete;
	explicit TCPBundleActivatorTest(const std::string& name);
	~TCPBundleActivatorTest() override;

	void testRegularFileAtSocketPathIsKept();
	void testSocketFileHasConfiguredMode();
	void testExportErrorDoesNotFailRegistration();
	void testListenerOnPortZeroExportsChosenPort();
	void testListenerThatURIsCannotCarryIsRejected();
	void testListenerWithServiceNameAsPortIsRejected();
	void testServiceRegisteredAfterStartIsExported();
	void testServiceIsExportedAgainAfterRestart();
	void testServiceUnregisteredDuringRegistrationIsNotExported();
	void testServiceRegisteredAgainDuringUnregistrationIsExported();
	void testServiceWithSameObjectIdRegisteredAgainIsExported();
	void testServiceRegisteredAgainConcurrentlyIsExported();
	void testServiceUnregisteredDuringExportIsRemoved();
	void testServiceRegisteredWhileObjectIsRemovedIsExported();
	void testStandardExceptionOfServiceIsLogged();
	void testServiceFactoryCanRegisterService();
	void testServiceRegisteredDuringStartIsExported();
	void testFailedStartLeavesNothingRegistered();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // TCPBundleActivatorTest_INCLUDED
