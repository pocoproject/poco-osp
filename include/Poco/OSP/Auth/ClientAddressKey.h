//
// ClientAddressKey.h
//
// Library: OSP
// Package: Auth
// Module:  ClientAddressKey
//
// The LoginThrottle key of a client address, the exempt addresses
// and the settings of a LoginThrottle. The functions use Poco::Net,
// so only code that links Poco::Net includes this header.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_ClientAddressKey_INCLUDED
#define OSP_Auth_ClientAddressKey_INCLUDED


#include "Poco/OSP/Auth/LoginThrottle.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/NetworkInterface.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Exception.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Types.h"
#include <algorithm>
#include <chrono>
#include <iterator>
#include <span>
#include <string>
#include <vector>


namespace Poco::OSP::Auth {


struct ExemptAddress
	/// An entry of exemptAddresses. Plain data.
{
	Poco::Net::IPAddress address;
		/// IPv4, or IPv6 (for a prefix: the /64 prefix).
	bool prefix64 = false;
		/// The entry is an IPv6 /64 prefix ("2001:db8:1:2::/64").
	Poco::UInt32 scope = 0;
		/// The interface index of a link-local IPv6 entry written with a scope id ("fe80::1%eth0"); 0: any interface.
};


using ExemptAddresses = std::vector<ExemptAddress>;
	/// The entries of the setting exemptAddresses (see parseExemptAddresses()).


struct LoginThrottleConfig
	/// The settings of one component's LoginThrottle. Plain data.
{
	LoginThrottle::Settings throttle;
	ExemptAddresses exemptAddresses;
};


namespace Impl {


inline constexpr unsigned PREFIX_LENGTH = 64;
inline constexpr unsigned PREFIX_BYTES = PREFIX_LENGTH / 8;


[[nodiscard]] inline Poco::Net::IPAddress unmappedAddress(const Poco::Net::IPAddress& address)
	/// Returns the IPv4 address of an IPv4-mapped IPv6 address, and any other address unchanged.
{
	if (!address.isV6() || !address.isIPv4Mapped()) return address;

	const auto bytes = address.toV6Bytes();
	const auto ipv4Bytes = std::span(bytes).last(Poco::Net::IPAddress::IPv4Size);
	return Poco::Net::IPAddress(ipv4Bytes.data(), Poco::Net::IPAddress::IPv4Size);
}


[[nodiscard]] inline Poco::Net::IPAddress prefix64(const Poco::Net::IPAddress& address)
	/// Returns the /64 prefix of an IPv6 address, without the scope id.
{
	auto bytes = address.toV6Bytes();
	std::fill(std::next(bytes.begin(), PREFIX_BYTES), bytes.end(), 0);
	return Poco::Net::IPAddress(bytes.data(), Poco::Net::IPAddress::IPv6Size);
}


[[nodiscard]] inline Poco::UInt32 interfaceIndex(const std::string& scope)
	/// Returns the index of the network interface that the scope id (a name or an index) names.
	/// Throws a Poco::Net::InterfaceNotFoundException if no such interface exists.
{
	unsigned index = 0;
	if (Poco::NumberParser::tryParseUnsigned(scope, index))
	{
		return Poco::Net::NetworkInterface::forIndex(index).index();
	}
	return Poco::Net::NetworkInterface::forName(scope, Poco::Net::NetworkInterface::IPv4_OR_IPv6).index();
}


[[nodiscard]] inline ExemptAddress parseExemptAddress(const std::string& entry, const std::string& key)
	/// Parses one entry of parseExemptAddresses().
{
	const auto invalid = [&entry, &key](const std::string& reason)
	{
		return Poco::InvalidArgumentException("The setting " + key + " has the entry \"" + entry + "\": " + reason);
	};

	ExemptAddress result;
	std::string address = entry;
	if (const auto slash = entry.find('/'); slash != std::string::npos)
	{
		unsigned length = 0;
		if (!Poco::NumberParser::tryParseUnsigned(entry.substr(slash + 1), length) || length != PREFIX_LENGTH)
		{
			throw invalid("a prefix must have the length " + Poco::NumberFormatter::format(PREFIX_LENGTH) + ".");
		}
		address.resize(slash);
		result.prefix64 = true;
	}
	std::string scope;
	const auto percent = address.find('%');
	if (percent != std::string::npos)
	{
		if (result.prefix64) throw invalid("a prefix cannot have a scope id.");
		scope = address.substr(percent + 1);
		address.resize(percent);
	}

	Poco::Net::IPAddress parsed;
	// The system parser also accepts IPv4 forms such as "127.1", "010.0.0.1" or "0x7f000001", which denote
	// another address than an operator reading the entry would expect; only the dotted-decimal form is taken.
	if (!Poco::Net::IPAddress::tryParse(address, parsed) || (parsed.isV4() && parsed.toString() != address))
	{
		throw invalid("it is not an IPv4 address, an IPv6 address or an IPv6 /64 prefix.");
	}
	parsed = unmappedAddress(parsed);
	if (result.prefix64)
	{
		if (!parsed.isV6()) throw invalid("a prefix must be an IPv6 prefix.");
		result.address = prefix64(parsed);
	}
	else result.address = parsed;

	if (percent != std::string::npos)
	{
		if (!parsed.isV6()) throw invalid("only an IPv6 address can have a scope id.");
		if (!parsed.isLinkLocal()) throw invalid("only a link-local IPv6 address can have a scope id.");
		if (scope.empty()) throw invalid("the scope id is empty.");
		try
		{
			result.scope = interfaceIndex(scope);
		}
		catch (const Poco::Net::InterfaceNotFoundException&)
		{
			throw invalid("the network interface " + scope + " does not exist.");
		}
	}
	return result;
}


[[nodiscard]] inline int intSetting(const Poco::Util::AbstractConfiguration& configuration, const std::string& key)
	/// Returns the integer value of the setting with the given key, which the configuration has.
	/// Throws a Poco::InvalidArgumentException naming the key and the value if the value is not an integer.
{
	try
	{
		return configuration.getInt(key);
	}
	catch (const Poco::SyntaxException&)
	{
		throw Poco::InvalidArgumentException(
			"The setting " + key + " is \"" + configuration.getString(key) + "\"; it must be an integer.");
	}
}


} // namespace Impl


[[nodiscard]] inline std::string clientAddressKey(const Poco::Net::IPAddress& address)
	/// Returns the LoginThrottle key of a client address. IPv4: the address ("192.0.2.7").
	/// IPv4-mapped IPv6: the IPv4 address. IPv6: the /64 prefix with the scope id removed,
	/// written "2001:db8:1:2::/64".
{
	const Poco::Net::IPAddress unmapped = Impl::unmappedAddress(address);
	if (unmapped.isV4()) return unmapped.toString();

	return Impl::prefix64(unmapped).toString() + "/" + Poco::NumberFormatter::format(Impl::PREFIX_LENGTH);
}


[[nodiscard]] inline ExemptAddresses parseExemptAddresses(const std::string& value, const std::string& key)
	/// Parses the value of the setting exemptAddresses with the given key. The entries are separated by
	/// commas or white space: an IPv4 address in dotted-decimal form, an IPv6 address, a link-local IPv6
	/// address with a scope id (%name or %index), or an IPv6 /64 prefix without a scope id. An IPv4-mapped
	/// IPv6 address is taken as its IPv4 address. The scope id is resolved to the interface index when the
	/// list is parsed.
	///
	/// Throws a Poco::InvalidArgumentException naming the key and the entry for an entry that is not
	/// an address or a prefix, a prefix length other than 64, a scope id on an IPv4 address, on a prefix
	/// or on an IPv6 address that is not link-local, an empty scope id, and a scope id whose network
	/// interface does not exist.
{
	ExemptAddresses result;
	const Poco::StringTokenizer entries(
		value, ", \t\r\n", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
	for (const auto& entry: entries)
	{
		result.push_back(Impl::parseExemptAddress(entry, key));
	}
	return result;
}


[[nodiscard]] inline bool isExemptAddress(const Poco::Net::IPAddress& client, const ExemptAddresses& exempt)
	/// Returns true if the client address matches an entry of exempt. An IPv4-mapped client address is
	/// taken as its IPv4 address; its scope id is kept, because link-local addresses on different links are
	/// different hosts. An entry without a scope id matches the client address on any interface, an entry
	/// with a scope id only a client on the same interface. A /64 entry matches a client whose first 64 bits
	/// are equal.
{
	const Poco::Net::IPAddress unmapped = Impl::unmappedAddress(client);
	return std::ranges::any_of(exempt, [&unmapped](const ExemptAddress& entry)
	{
		if (entry.prefix64) return unmapped.isV6() && Impl::prefix64(unmapped) == entry.address;
		if (entry.scope != 0 && entry.scope != unmapped.scope()) return false;
		return unmapped.toBytes() == entry.address.toBytes();
	});
}


[[nodiscard]] inline LoginThrottleConfig loginThrottleSettings(
	const Poco::Util::AbstractConfiguration& configuration,
	const std::string& prefix,
	const LoginThrottleConfig& defaults = {})
	/// Reads the settings <prefix>maxAttempts, <prefix>cooldownSeconds and <prefix>exemptAddresses
	/// (see parseExemptAddresses()). A setting that the configuration does not have keeps the value of defaults.
	///
	/// Throws a Poco::InvalidArgumentException naming the setting and the value for a value that is not
	/// an integer, a negative maxAttempts, a cooldownSeconds outside LoginThrottle::MIN_COOLDOWN to
	/// LoginThrottle::MAX_COOLDOWN while maxAttempts is greater than 0, and the errors of parseExemptAddresses().
{
	LoginThrottleConfig config = defaults;
	const auto valueText = [&configuration](const std::string& key, const std::string& defaultValue)
	{
		if (configuration.hasProperty(key)) return "\"" + configuration.getString(key) + "\"";
		return defaultValue + " (the default)";
	};

	const std::string maxAttemptsKey = prefix + "maxAttempts";
	if (configuration.hasProperty(maxAttemptsKey))
	{
		config.throttle.maxAttempts = Impl::intSetting(configuration, maxAttemptsKey);
	}
	const std::string cooldownSecondsKey = prefix + "cooldownSeconds";
	if (configuration.hasProperty(cooldownSecondsKey))
	{
		config.throttle.cooldown = std::chrono::seconds(Impl::intSetting(configuration, cooldownSecondsKey));
	}
	const std::string exemptAddressesKey = prefix + "exemptAddresses";
	if (configuration.hasProperty(exemptAddressesKey))
	{
		config.exemptAddresses = parseExemptAddresses(configuration.getString(exemptAddressesKey), exemptAddressesKey);
	}

	if (config.throttle.maxAttempts < 0)
	{
		throw Poco::InvalidArgumentException("The setting " + maxAttemptsKey + " is " +
			valueText(maxAttemptsKey, Poco::NumberFormatter::format(config.throttle.maxAttempts)) +
			"; it must be 0 or greater.");
	}
	const auto cooldown = config.throttle.cooldown;
	if (config.throttle.maxAttempts > 0 && !LoginThrottle::isValidCooldown(cooldown))
	{
		throw Poco::InvalidArgumentException("The setting " + cooldownSecondsKey + " is " +
			valueText(cooldownSecondsKey, Poco::NumberFormatter::format(cooldown.count())) + "; it must be from " +
			Poco::NumberFormatter::format(LoginThrottle::MIN_COOLDOWN.count()) + " to " +
			Poco::NumberFormatter::format(LoginThrottle::MAX_COOLDOWN.count()) + " while " + maxAttemptsKey +
			" is greater than 0.");
	}
	return config;
}


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_ClientAddressKey_INCLUDED
