//
// LanguageTagTest.h
//
// Definition of the LanguageTagTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LanguageTagTest_INCLUDED
#define LanguageTagTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class LanguageTagTest: public CppUnit::TestCase
{
public:
	LanguageTagTest() = delete;
	explicit LanguageTagTest(const std::string& name);
	~LanguageTagTest() override;

	void testLocaleNameEncodingAndModifierAreIgnored();
	void testToStringWithoutSubTagHasNoSeparator();
	void testCAndPosixLocalesGiveDefaultLanguage();
	void testTagIsSplitIntoPrimaryAndSubTag();
	void testDefaultTagIsTakenFromEnvironment();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // LanguageTagTest_INCLUDED
