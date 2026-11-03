//
// LoginFailures.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  LoginFailures
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/LoginFailures.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Mutex.h"
#include <algorithm>
#include <chrono>


namespace Poco::OSP::Shell {


LoginFailures::LoginFailures(std::chrono::milliseconds firstDelay):
	_firstDelay(firstDelay),
	_failures(MAX_ADDRESSES, std::chrono::duration_cast<std::chrono::milliseconds>(FORGET_AFTER).count())
{
}


LoginFailures::~LoginFailures() = default;


std::chrono::milliseconds LoginFailures::recordFailure(const Poco::Net::IPAddress& address)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	int failures = 1;
	if (auto pFailures = _failures.get(address); pFailures != nullptr)
	{
		failures = *pFailures + 1;
	}
	_failures.add(address, failures);

	const std::chrono::milliseconds maxDelay = _firstDelay * MAX_DELAY_FACTOR;
	std::chrono::milliseconds delay = _firstDelay;
	for (int i = 1; i < failures && delay < maxDelay; ++i)
	{
		delay *= 2;
	}
	return std::min(delay, maxDelay);
}


} // namespace Poco::OSP::Shell
