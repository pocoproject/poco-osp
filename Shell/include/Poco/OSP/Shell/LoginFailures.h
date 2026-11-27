//
// LoginFailures.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginFailures
//
// Definition of the LoginFailures class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_LoginFailures_INCLUDED
#define OSP_Shell_LoginFailures_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/ExpireLRUCache.h"
#include "Poco/Mutex.h"
#include <chrono>
#include <cstddef>


namespace Poco::OSP::Shell {


class OSPShell_API LoginFailures
	/// LoginFailures counts the rejected shell logins of each client address,
	/// so that the delay after a rejected login grows for a client that tries
	/// again, also over a new connection.
	///
	/// An address is forgotten when no login from it has been rejected for
	/// ten minutes. At most 1024 addresses are kept; the address with the
	/// oldest rejected login is forgotten first.
	///
	/// All member functions can be called from several threads at once.
{
public:
	explicit LoginFailures(std::chrono::milliseconds firstDelay = DEFAULT_FIRST_DELAY);
		/// Creates the LoginFailures without any rejected login. The delay
		/// after the first rejected login from an address is firstDelay.

	~LoginFailures();
		/// Destroys the LoginFailures.

	LoginFailures(const LoginFailures&) = delete;
	LoginFailures(LoginFailures&&) = delete;
	LoginFailures& operator = (const LoginFailures&) = delete;
	LoginFailures& operator = (LoginFailures&&) = delete;

	[[nodiscard]] std::chrono::milliseconds recordFailure(const Poco::Net::IPAddress& address);
		/// Records a rejected login from the given address and returns the delay
		/// before the rejection is answered: the first delay after the first
		/// rejected login, doubled after each further one, at most MAX_DELAY_FACTOR
		/// times the first delay.

	static constexpr std::chrono::milliseconds DEFAULT_FIRST_DELAY{1000};
		/// The default delay after the first rejected login from an address.

	static constexpr int MAX_DELAY_FACTOR = 16;
		/// The longest delay after a rejected login, as a multiple of the first delay.

private:
	static constexpr std::size_t MAX_ADDRESSES = 1024;
	static constexpr std::chrono::minutes FORGET_AFTER{10};

	const std::chrono::milliseconds _firstDelay;
	Poco::FastMutex _mutex;
	Poco::ExpireLRUCache<Poco::Net::IPAddress, int> _failures;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_LoginFailures_INCLUDED
