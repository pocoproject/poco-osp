//
// LanguageTagTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LanguageTagTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/Environment.h"
#include <stdlib.h>
#include <string>
#include <vector>


using Poco::OSP::LanguageTag;
using Poco::Environment;


LanguageTagTest::LanguageTagTest(const std::string& name): CppUnit::TestCase(name)
{
}


LanguageTagTest::~LanguageTagTest() = default;


void LanguageTagTest::testLocaleNameEncodingAndModifierAreIgnored()
{
	LanguageTag withEncoding("de_AT.UTF-8");
	assertEqual ("de", withEncoding.primaryTag());
	assertEqual ("AT", withEncoding.subTags());

	LanguageTag withModifier("de_AT@euro");
	assertEqual ("de", withModifier.primaryTag());
	assertEqual ("AT", withModifier.subTags());

	LanguageTag languageOnly("sl.UTF-8");
	assertEqual ("sl", languageOnly.primaryTag());
	assertEqual ("", languageOnly.subTags());
}


void LanguageTagTest::testToStringWithoutSubTagHasNoSeparator()
{
	assertEqual ("de-AT", LanguageTag("de", "AT").toString());
	assertEqual ("sl", LanguageTag("sl", "").toString());
}


void LanguageTagTest::testCAndPosixLocalesGiveDefaultLanguage()
{
#if defined(POCO_OS_FAMILY_UNIX)
	const bool hadLang = Environment::has("LANG");
	const std::string lang = Environment::get("LANG", "");
	const std::vector<std::string> locales = {"C", "C.UTF-8", "POSIX", ""};
	for (const auto& locale: locales)
	{
		Environment::set("LANG", locale);
		LanguageTag tag;
		if (hadLang)
			Environment::set("LANG", lang);
		else
			::unsetenv("LANG");
		assertEqual ("en", tag.primaryTag());
		assertEqual ("US", tag.subTags());
	}
#endif
}


void LanguageTagTest::testTagIsSplitIntoPrimaryAndSubTag()
{
	LanguageTag hyphen("de-AT");
	assertEqual ("de", hyphen.primaryTag());
	assertEqual ("AT", hyphen.subTags());
	assertEqual ("de-AT", hyphen.toString());

	LanguageTag localeName("DE_at.UTF-8");
	assertEqual ("de", localeName.primaryTag());
	assertEqual ("AT", localeName.subTags());
	assertEqual ("de-AT", localeName.toString());

	LanguageTag empty("");
	assertEqual ("", empty.primaryTag());
	assertEqual ("", empty.subTags());
	assertEqual ("", empty.toString());
}


void LanguageTagTest::testDefaultTagIsTakenFromEnvironment()
{
#if defined(POCO_OS_FAMILY_UNIX)
	const bool hadLang = Environment::has("LANG");
	const std::string lang = Environment::get("LANG", "");
	Environment::set("LANG", "fr_CA.UTF-8");
	LanguageTag tag;
	if (hadLang)
		Environment::set("LANG", lang);
	else
		::unsetenv("LANG");
	assertEqual ("fr", tag.primaryTag());
	assertEqual ("CA", tag.subTags());
#endif
}


void LanguageTagTest::setUp()
{
}


void LanguageTagTest::tearDown()
{
}


CppUnit::Test* LanguageTagTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LanguageTagTest");

	CppUnit_addTest(pSuite, LanguageTagTest, testLocaleNameEncodingAndModifierAreIgnored);
	CppUnit_addTest(pSuite, LanguageTagTest, testToStringWithoutSubTagHasNoSeparator);
	CppUnit_addTest(pSuite, LanguageTagTest, testCAndPosixLocalesGiveDefaultLanguage);
	CppUnit_addTest(pSuite, LanguageTagTest, testTagIsSplitIntoPrimaryAndSubTag);
	CppUnit_addTest(pSuite, LanguageTagTest, testDefaultTagIsTakenFromEnvironment);

	return pSuite;
}
