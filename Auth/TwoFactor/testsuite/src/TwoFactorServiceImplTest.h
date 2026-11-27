//
// TwoFactorServiceImplTest.h
//
// Definition of the TwoFactorServiceImplTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TwoFactorServiceImplTest_INCLUDED
#define TwoFactorServiceImplTest_INCLUDED


#include "Poco/OSP/Auth/TwoFactor/TwoFactor.h"
#include "CppUnit/TestCase.h"


class TwoFactorServiceImplTest: public CppUnit::TestCase
{
public:
	TwoFactorServiceImplTest() = delete;
	explicit TwoFactorServiceImplTest(const std::string& name);
	~TwoFactorServiceImplTest() override;

	void testEnableAndDisable();
	void testCodeOfCurrentStepIsAccepted();
	void testCodesOfAdjacentStepsAreAccepted();
	void testCodeOutsideWindowIsRejected();
	void testCodeOfWrongLengthIsRejected();
	void testValidationWithoutSecretThrows();
	void testAcceptedCodeCannotBeReused();
	void testConcurrentValidationAcceptsCodeOnce();
	void testFailuresLockOutUser();
	void testLockOutIsLogged();
	void testSuccessResetsFailureCount();
	void testFailureAfterLockOutLocksOutAgain();
	void testSecretIsBoundToUser();
	void testEqualSecretsAreStoredDifferently();
	void testOtherPassphraseCannotReadSecret();
	void testSecretWithoutVersionIsReadAndStoredInCurrentFormat();
	void testSecretWithoutVersionIsStoredInCurrentFormatAfterValidation();
	void testValidationRemovesLastCodeOfOlderVersion();
	void testDisableRemovesEveryTwoFactorAttribute();
	void testEmptyKeySaltIsRejected();
	void testCodeDigitsAndSecretLengthAreChecked();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // TwoFactorServiceImplTest_INCLUDED
