//
// WebSessionManagerTest.h
//
// Definition of the WebSessionManagerTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebSessionManagerTest_INCLUDED
#define WebSessionManagerTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"
#include <memory>


class TemporaryBundle;


class WebSessionManagerTest: public CppUnit::TestCase
{
public:
	WebSessionManagerTest() = delete;
	explicit WebSessionManagerTest(const std::string& name);
	~WebSessionManagerTest() override;

	void setUp() override;
	void tearDown() override;

	void testSessionCookieAttributes();
	void testCookiePathFallsBackToDefaultPath();
	void testCookiesSecureOverHTTPS();
	void testCookiesSecureWhenConfigured();
	void testGetCookieSameSite();
#ifdef POCO_TEST_DEPRECATED
	void testDeprecatedSameSiteGetter();
#endif
	void testAddressMismatchRemovesSession();
	void testRenewReplacesSession();
	void testCSRFCookieCreatesMissingToken();
	void testRemoveForUserRemovesOnlyThatUser();
	void testRemoveForUserWhileAttributesChange();
	void testBundleStoppingClearsCachedSessions();
	void testBundleStoppingWhileAttributesChange();
	void testBundleStoppingClearsSessionsNoLongerCached();
	void testBundleStoppingClearsSessionsLoadedFromStore();
	void testSessionOutlivesManagerAndBundleLoader();
	void testBundleStoppingWhileSessionsComeAndGo();
	void testReplacedAttributeReleasesSessionOutsideLock();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<TemporaryBundle> _pBundle;
};


#endif // WebSessionManagerTest_INCLUDED
