//
// ClientAddressKeyTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ClientAddressKeyTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Auth/ClientAddressKey.h"
#include "Poco/OSP/Auth/LoginThrottle.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetworkInterface.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include "Poco/Types.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>


using Poco::OSP::Auth::LoginThrottleConfig;
using Poco::OSP::Auth::clientAddressKey;
using Poco::OSP::Auth::isExemptAddress;
using Poco::OSP::Auth::loginThrottleSettings;
using Poco::OSP::Auth::parseExemptAddresses;
using Poco::Net::IPAddress;
using Poco::Net::NetworkInterface;
using Poco::Util::MapConfiguration;
using namespace std::chrono_literals;


namespace
{
	[[nodiscard]] NetworkInterface loopbackInterface()
	{
		const NetworkInterface::List interfaces = NetworkInterface::list(false, false);
		const auto it = std::ranges::find_if(interfaces,
			[](const NetworkInterface& iface) { return iface.isLoopback(); });
		if (it == interfaces.end()) throw Poco::NotFoundException("loopback network interface");
		return *it;
	}

	[[nodiscard]] bool isSingleEntry(const std::string& text)
		/// Returns true if the text contains no separator of the setting exemptAddresses. On Windows, the
		/// name of a network interface is the friendly name of the adapter, such as "Loopback Pseudo-Interface 1".
	{
		return text.find_first_of(", \t\r\n") == std::string::npos;
	}

	[[nodiscard]] IPAddress scoped(const std::string& address, Poco::UInt32 scope)
	{
		const IPAddress unscoped(address);
		return IPAddress(unscoped.addr(), unscoped.length(), scope);
	}

	[[nodiscard]] std::string exemptAddressesError(const std::string& value)
		/// Returns the message of the exception that parseExemptAddresses() throws for the value,
		/// or an empty string if it throws none.
	{
		try
		{
			static_cast<void>(parseExemptAddresses(value, "test.exemptAddresses"));
		}
		catch (const Poco::InvalidArgumentException& exc)
		{
			return exc.message();
		}
		return {};
	}

	[[nodiscard]] std::string settingsError(const MapConfiguration& configuration)
		/// Returns the message of the exception that loginThrottleSettings() throws for the configuration,
		/// or an empty string if it throws none.
	{
		try
		{
			static_cast<void>(loginThrottleSettings(configuration, "test.login."));
		}
		catch (const Poco::InvalidArgumentException& exc)
		{
			return exc.message();
		}
		return {};
	}

	[[nodiscard]] bool contains(const std::string& text, const std::string& part)
	{
		return text.find(part) != std::string::npos;
	}
}


ClientAddressKeyTest::ClientAddressKeyTest(const std::string& name): CppUnit::TestCase(name)
{
}


ClientAddressKeyTest::~ClientAddressKeyTest() = default;


void ClientAddressKeyTest::testKeyOfIPv4Address()
{
	assertEqual ("192.0.2.7", clientAddressKey(IPAddress("192.0.2.7")));
}


void ClientAddressKeyTest::testKeyOfIPv4MappedAddressIsIPv4Address()
{
	assertEqual ("192.0.2.7", clientAddressKey(IPAddress("::ffff:192.0.2.7")));
}


void ClientAddressKeyTest::testKeyOfIPv6AddressIsPrefix64()
{
	assertEqual ("2001:db8:1:2::/64", clientAddressKey(IPAddress("2001:db8:1:2:aaaa:bbbb:cccc:dddd")));
	assertEqual ("2001:db8:1:2::/64", clientAddressKey(IPAddress("2001:db8:1:2::1")));
	assertEqual ("2001:db8:1:3::/64", clientAddressKey(IPAddress("2001:db8:1:3::1")));
	assertEqual ("::/64", clientAddressKey(IPAddress("::1")));
}


void ClientAddressKeyTest::testKeyOfIPv6AddressHasNoScopeId()
{
	const NetworkInterface loopback = loopbackInterface();
	assertEqual ("fe80::/64", clientAddressKey(scoped("fe80::1", loopback.index())));
	assertEqual ("fe80::/64", clientAddressKey(scoped("fe80::2", loopback.index() + 1)));
}


void ClientAddressKeyTest::testExemptIPv4Address()
{
	const auto exempt = parseExemptAddresses("192.0.2.7", "test.exemptAddresses");
	assertTrue (isExemptAddress(IPAddress("192.0.2.7"), exempt));
	assertTrue (isExemptAddress(IPAddress("::ffff:192.0.2.7"), exempt));
	assertFalse (isExemptAddress(IPAddress("192.0.2.8"), exempt));
	assertFalse (isExemptAddress(IPAddress("::c000:207"), exempt));
	assertFalse (isExemptAddress(IPAddress("192.0.2.7"), {}));
}


void ClientAddressKeyTest::testExemptIPv4MappedEntryIsIPv4Address()
{
	const auto exempt = parseExemptAddresses("::ffff:192.0.2.9", "test.exemptAddresses");
	assertTrue (exempt.at(0).address.isV4());
	assertTrue (isExemptAddress(IPAddress("192.0.2.9"), exempt));
	assertTrue (isExemptAddress(IPAddress("::ffff:192.0.2.9"), exempt));
}


void ClientAddressKeyTest::testExemptPrefix64()
{
	const auto exempt = parseExemptAddresses("2001:db8:1:2::9/64", "test.exemptAddresses");
	assertTrue (exempt.at(0).prefix64);
	assertEqual ("2001:db8:1:2::", exempt.at(0).address.toString());
	assertTrue (isExemptAddress(IPAddress("2001:db8:1:2::5"), exempt));
	assertTrue (isExemptAddress(IPAddress("2001:db8:1:2:ffff:ffff:ffff:ffff"), exempt));
	assertTrue (isExemptAddress(scoped("2001:db8:1:2::5", 1), exempt));
	assertFalse (isExemptAddress(IPAddress("2001:db8:1:3::5"), exempt));
	assertFalse (isExemptAddress(IPAddress("192.0.2.1"), exempt));
}


void ClientAddressKeyTest::testExemptAddressWithoutScopeIdMatchesAnyInterface()
{
	const NetworkInterface loopback = loopbackInterface();
	const auto exempt = parseExemptAddresses("fe80::1", "test.exemptAddresses");
	assertTrue (isExemptAddress(IPAddress("fe80::1"), exempt));
	assertTrue (isExemptAddress(scoped("fe80::1", loopback.index()), exempt));
	assertTrue (isExemptAddress(scoped("fe80::1", loopback.index() + 1), exempt));
	assertFalse (isExemptAddress(IPAddress("fe80::2"), exempt));
}


void ClientAddressKeyTest::testExemptAddressWithScopeIdMatchesOnlyThatInterface()
{
	const NetworkInterface loopback = loopbackInterface();
	std::vector<std::string> entries = {"fe80::1%" + std::to_string(loopback.index())};
	if (isSingleEntry(loopback.name())) entries.push_back("fe80::1%" + loopback.name());
	else warnmsg("The name of the loopback interface contains a separator, scope id by name case skipped.");
	for (const auto& entry: entries)
	{
		const auto exempt = parseExemptAddresses(entry, "test.exemptAddresses");
		assertEqual (loopback.index(), exempt.at(0).scope);
		assertTrue (isExemptAddress(scoped("fe80::1", loopback.index()), exempt));
		assertFalse (isExemptAddress(scoped("fe80::1", loopback.index() + 1), exempt));
		assertFalse (isExemptAddress(IPAddress("fe80::1"), exempt));
		assertFalse (isExemptAddress(scoped("fe80::2", loopback.index()), exempt));
	}
}


void ClientAddressKeyTest::testExemptAddressesAreSeparatedByCommasOrWhiteSpace()
{
	const auto exempt =
		parseExemptAddresses(" 192.0.2.1, 192.0.2.2  2001:db8::/64,,192.0.2.3 ", "test.exemptAddresses");
	assertEqual (4, static_cast<int>(exempt.size()));
	assertTrue (isExemptAddress(IPAddress("192.0.2.2"), exempt));
	assertTrue (isExemptAddress(IPAddress("2001:db8::5"), exempt));
	assertTrue (isExemptAddress(IPAddress("192.0.2.3"), exempt));
	assertTrue (parseExemptAddresses("", "test.exemptAddresses").empty());

	const auto lines = parseExemptAddresses("192.0.2.1\n192.0.2.2\r\n\t192.0.2.3", "test.exemptAddresses");
	assertEqual (3, static_cast<int>(lines.size()));
	assertTrue (isExemptAddress(IPAddress("192.0.2.2"), lines));
	assertTrue (isExemptAddress(IPAddress("192.0.2.3"), lines));
}


void ClientAddressKeyTest::testInvalidExemptAddressesAreRejected()
{
	const NetworkInterface loopback = loopbackInterface();
	const std::string index = std::to_string(loopback.index());
	// The entries with scope are rejected before the interface is looked up, so the index can stand in for a
	// name that cannot be written in the list.
	const std::string scope = isSingleEntry(loopback.name()) ? loopback.name() : index;
	const std::vector<std::string> entries = {
		"localhost",
		"192.0.2.256",
		"2001:db8::/48",
		"2001:db8::/",
		"192.0.2.0/64",
		"010.0.0.1",
		"127.1",
		"192.0.2",
		"1",
		"0x7f000001",
		"192.0.2.1%" + index,
		"::ffff:192.0.2.1%" + index,
		"169.254.0.1%" + index,
		"::ffff:169.254.0.1%" + index,
		"2001:db8::1%" + index,
		"2001:db8::1%" + scope,
		"ff02::1%" + scope,
		"fe80::%" + scope + "/64",
		"fe80::1%no-such-interface",
		"fe80::1%4294967295",
		"fe80::1%"
	};
	for (const auto& entry: entries)
	{
		const std::string error = exemptAddressesError("192.0.2.1, " + entry);
		assertTrue (contains(error, "test.exemptAddresses"));
		assertTrue (contains(error, "\"" + entry + "\""));
	}
	assertTrue (contains(exemptAddressesError("192.0.2.1\tjunk"), "\"junk\""));
	assertTrue (contains(exemptAddressesError("fe80::1%"), "the scope id is empty."));
}


void ClientAddressKeyTest::testSettingsKeepDefaults()
{
	const LoginThrottleConfig builtIn = loginThrottleSettings(*Poco::AutoPtr<MapConfiguration>(new MapConfiguration),
		"test.login.");
	assertEqual (10, builtIn.throttle.maxAttempts);
	assertTrue (builtIn.throttle.cooldown == 60s);
	assertTrue (builtIn.exemptAddresses.empty());

	LoginThrottleConfig defaults;
	defaults.throttle.maxAttempts = 3;
	defaults.throttle.cooldown = 5s;
	defaults.exemptAddresses = parseExemptAddresses("192.0.2.1", "test.exemptAddresses");
	Poco::AutoPtr<MapConfiguration> pConfiguration = new MapConfiguration;
	pConfiguration->setString("other.login.maxAttempts", "7");
	const LoginThrottleConfig config = loginThrottleSettings(*pConfiguration, "test.login.", defaults);
	assertEqual (3, config.throttle.maxAttempts);
	assertTrue (config.throttle.cooldown == 5s);
	assertTrue (isExemptAddress(IPAddress("192.0.2.1"), config.exemptAddresses));
}


void ClientAddressKeyTest::testSettingsAreRead()
{
	LoginThrottleConfig defaults;
	defaults.exemptAddresses = parseExemptAddresses("192.0.2.1", "test.exemptAddresses");
	Poco::AutoPtr<MapConfiguration> pConfiguration = new MapConfiguration;
	pConfiguration->setString("test.login.maxAttempts", "4");
	pConfiguration->setString("test.login.cooldownSeconds", "86400");
	pConfiguration->setString("test.login.cooldown", "5");
	pConfiguration->setString("test.login.exemptAddresses", "192.0.2.2");
	const LoginThrottleConfig config = loginThrottleSettings(*pConfiguration, "test.login.", defaults);
	assertEqual (4, config.throttle.maxAttempts);
	assertTrue (config.throttle.cooldown == 86400s);
	assertFalse (isExemptAddress(IPAddress("192.0.2.1"), config.exemptAddresses));
	assertTrue (isExemptAddress(IPAddress("192.0.2.2"), config.exemptAddresses));

	pConfiguration->setString("test.login.maxAttempts", "0");
	pConfiguration->setString("test.login.cooldownSeconds", "0");
	pConfiguration->setString("test.login.exemptAddresses", "");
	const LoginThrottleConfig off = loginThrottleSettings(*pConfiguration, "test.login.", defaults);
	assertEqual (0, off.throttle.maxAttempts);
	assertTrue (off.exemptAddresses.empty());
}


void ClientAddressKeyTest::testInvalidSettingsAreRejected()
{
	struct Setting
	{
		std::string key;
		std::string value;
	};
	const std::vector<Setting> settings = {
		{.key = "test.login.maxAttempts", .value = "ten"},
		{.key = "test.login.maxAttempts", .value = "99999999999"},
		{.key = "test.login.maxAttempts", .value = "-1"},
		{.key = "test.login.maxAttempts", .value = "0xFFFFFFFF"},
		{.key = "test.login.cooldownSeconds", .value = "1m"},
		{.key = "test.login.cooldownSeconds", .value = "abc"},
		{.key = "test.login.cooldownSeconds", .value = "0"},
		{.key = "test.login.cooldownSeconds", .value = "-5"},
		{.key = "test.login.cooldownSeconds", .value = "86401"},
		{.key = "test.login.exemptAddresses", .value = "192.0.2.1/64"}
	};
	for (const auto& setting: settings)
	{
		Poco::AutoPtr<MapConfiguration> pConfiguration = new MapConfiguration;
		pConfiguration->setString(setting.key, setting.value);
		const std::string error = settingsError(*pConfiguration);
		assertTrue (contains(error, setting.key));
		assertTrue (contains(error, "\"" + setting.value + "\""));
	}
}


void ClientAddressKeyTest::testInvalidDefaultIsNamedAsDefault()
{
	LoginThrottleConfig defaults;
	defaults.throttle.maxAttempts = 0;
	defaults.throttle.cooldown = 0s;
	Poco::AutoPtr<MapConfiguration> pConfiguration = new MapConfiguration;
	pConfiguration->setString("test.login.maxAttempts", "10");
	try
	{
		static_cast<void>(loginThrottleSettings(*pConfiguration, "test.login.", defaults));
		fail("a default cooldownSeconds of 0 must be rejected");
	}
	catch (const Poco::InvalidArgumentException& exc)
	{
		assertTrue (contains(exc.message(), "test.login.cooldownSeconds is 0 (the default)"));
	}
}


void ClientAddressKeyTest::setUp()
{
}


void ClientAddressKeyTest::tearDown()
{
}


CppUnit::Test* ClientAddressKeyTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ClientAddressKeyTest");

	CppUnit_addTest(pSuite, ClientAddressKeyTest, testKeyOfIPv4Address);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testKeyOfIPv4MappedAddressIsIPv4Address);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testKeyOfIPv6AddressIsPrefix64);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testKeyOfIPv6AddressHasNoScopeId);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptIPv4Address);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptIPv4MappedEntryIsIPv4Address);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptPrefix64);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptAddressWithoutScopeIdMatchesAnyInterface);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptAddressWithScopeIdMatchesOnlyThatInterface);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testExemptAddressesAreSeparatedByCommasOrWhiteSpace);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testInvalidExemptAddressesAreRejected);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testSettingsKeepDefaults);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testSettingsAreRead);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testInvalidSettingsAreRejected);
	CppUnit_addTest(pSuite, ClientAddressKeyTest, testInvalidDefaultIsNamedAsDefault);

	return pSuite;
}
