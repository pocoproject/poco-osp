//
// ClientAddressKeyTest.h
//
// Definition of the ClientAddressKeyTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ClientAddressKeyTest_INCLUDED
#define ClientAddressKeyTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class ClientAddressKeyTest: public CppUnit::TestCase
{
public:
	ClientAddressKeyTest() = delete;
	explicit ClientAddressKeyTest(const std::string& name);
	~ClientAddressKeyTest() override;

	void testKeyOfIPv4Address();
	void testKeyOfIPv4MappedAddressIsIPv4Address();
	void testKeyOfIPv6AddressIsPrefix64();
	void testKeyOfIPv6AddressHasNoScopeId();
	void testExemptIPv4Address();
	void testExemptIPv4MappedEntryIsIPv4Address();
	void testExemptPrefix64();
	void testExemptAddressWithoutScopeIdMatchesAnyInterface();
	void testExemptAddressWithScopeIdMatchesOnlyThatInterface();
	void testExemptAddressesAreSeparatedByCommasOrWhiteSpace();
	void testInvalidExemptAddressesAreRejected();
	void testSettingsKeepDefaults();
	void testSettingsAreRead();
	void testInvalidSettingsAreRejected();
	void testInvalidDefaultIsNamedAsDefault();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // ClientAddressKeyTest_INCLUDED
