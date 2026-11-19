//
// SecureMailDeliveryServiceImplTest.h
//
// Definition of the SecureMailDeliveryServiceImplTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SecureMailDeliveryServiceImplTest_INCLUDED
#define SecureMailDeliveryServiceImplTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class SecureMailDeliveryServiceImplTest: public CppUnit::TestCase
{
public:
	SecureMailDeliveryServiceImplTest() = delete;
	explicit SecureMailDeliveryServiceImplTest(const std::string& name);
	~SecureMailDeliveryServiceImplTest() override;

	void testDestroyWhileConnecting();
	void testRefusedStartTLSFailsDelivery();
	void testStartTLSWithTrustedCertificate();
	void testStartTLSRejectsUntrustedCertificate();
	void testApplicationCertificateHandlerDoesNotAcceptUntrustedCertificate();
	void testContextVerifiesHostNameByDefault();
	void testCertificateForConfiguredHostNameIsAccepted();
	void testCertificateForOtherHostNameIsRejected();
	void testOtherHostNameIsAcceptedWithoutHostNameVerification();
	void testServerWithoutTLS12IsRefused();
	void testActivatorAppliesCALocation();
	void testHandshakeTimesOut();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SecureMailDeliveryServiceImplTest_INCLUDED
