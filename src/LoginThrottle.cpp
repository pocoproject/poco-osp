//
// LoginThrottle.cpp
//
// Library: OSP
// Package: Auth
// Module:  LoginThrottle
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/LoginThrottle.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/NumberFormatter.h"
#include "Poco/UTF8String.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>


using namespace std::string_literals;


namespace Poco::OSP::Auth {


LoginThrottle::Attempt::Attempt(std::string key):
	_key(std::move(key))
{
}


LoginThrottle::Attempt::Attempt(LoginThrottle& throttle, std::string key, std::uint64_t windowId):
	_pThrottle(&throttle),
	_key(std::move(key)),
	_windowId(windowId)
{
}


LoginThrottle::Attempt::Attempt(std::string key, std::chrono::seconds retryAfter, bool firstRefusal):
	_key(std::move(key)),
	_retryAfter(retryAfter),
	_admitted(false),
	_firstRefusal(firstRefusal)
{
}


LoginThrottle::Attempt::Attempt(Attempt&& attempt) noexcept:
	_pThrottle(std::exchange(attempt._pThrottle, nullptr)),
	_key(std::move(attempt._key)),
	_windowId(attempt._windowId),
	_retryAfter(attempt._retryAfter),
	_admitted(attempt._admitted),
	_firstRefusal(attempt._firstRefusal)
{
}


LoginThrottle::Attempt::~Attempt() = default;


LoginThrottle::Attempt& LoginThrottle::Attempt::operator = (Attempt&& attempt) noexcept
{
	if (this != &attempt)
	{
		_pThrottle = std::exchange(attempt._pThrottle, nullptr);
		_key = std::move(attempt._key);
		_windowId = attempt._windowId;
		_retryAfter = attempt._retryAfter;
		_admitted = attempt._admitted;
		_firstRefusal = attempt._firstRefusal;
	}
	return *this;
}


void LoginThrottle::Attempt::succeeded()
{
	if (_pThrottle != nullptr)
	{
		std::exchange(_pThrottle, nullptr)->refund(_key, _windowId);
	}
}


LoginThrottle::LoginThrottle(Settings settings, Clock clock):
	_settings(settings),
	_clock(clock != nullptr ? std::move(clock) : Clock([]() { return std::chrono::steady_clock::now(); })),
	_logger(Poco::Logger::get("osp.core.LoginThrottle"))
{
	if (_settings.maxAttempts < 0)
	{
		throw Poco::InvalidArgumentException("The login throttle setting maxAttempts is " +
			Poco::NumberFormatter::format(_settings.maxAttempts) + "; it must be 0 or greater.");
	}
	if (enabled() && !isValidCooldown(_settings.cooldown))
	{
		throw Poco::InvalidArgumentException("The login throttle setting cooldown is " +
			Poco::NumberFormatter::format(_settings.cooldown.count()) + " s; it must be from " +
			Poco::NumberFormatter::format(MIN_COOLDOWN.count()) + " to " +
			Poco::NumberFormatter::format(MAX_COOLDOWN.count()) + " s while maxAttempts is greater than 0.");
	}
}


LoginThrottle::~LoginThrottle() = default;


LoginThrottle::Attempt LoginThrottle::begin(const std::string& key)
{
	if (!enabled()) return Attempt(key);

	Poco::FastMutex::ScopedLock lock(_mutex);

	const auto now = _clock();
	auto it = _windows.find(key);
	if (it == _windows.end())
	{
		makeRoom(now);
		// The key joins _order only after the emplace succeeded, so that an exception cannot leave a key in _order
		// without a window.
		Order node{key};
		it = _windows.emplace(key, newWindow(now, node.begin())).first;
		_order.splice(_order.end(), node);
	}
	else if (it->second.end <= now)
	{
		_order.splice(_order.end(), _order, it->second.itOrder);
		it->second = newWindow(now, it->second.itOrder);
	}

	Window& window = it->second;
	if (window.counted >= _settings.maxAttempts)
	{
		const bool firstRefusal = !window.refused;
		window.refused = true;
		// The window has not ended, so the rounded-up rest is at least one second.
		return Attempt(key, std::chrono::ceil<std::chrono::seconds>(window.end - now), firstRefusal);
	}
	++window.counted;
	return Attempt(*this, key, window.id);
}


LoginThrottle::Attempt LoginThrottle::exempt(const std::string& key)
{
	return Attempt(key);
}


void LoginThrottle::logRefusal(Poco::Logger& logger, const Attempt& refusal, const std::string& userName)
{
	poco_assert (!refusal.admitted());

	const auto retryAfter = refusal.retryAfter().count();
	if (refusal.firstRefusal())
	{
		logger.warning("Too many failed password checks from %s: checks are refused for %?d s."s,
			refusal.key(), retryAfter);
	}
	if (logger.debug())
	{
		logger.debug("Password check of user %s from %s refused for %?d s."s,
			Poco::UTF8::escape(userName), refusal.key(), retryAfter);
	}
}


LoginThrottle::Window LoginThrottle::newWindow(std::chrono::steady_clock::time_point now, Order::iterator itOrder)
{
	return Window{.end = now + _settings.cooldown, .id = ++_lastWindow, .itOrder = itOrder};
}


void LoginThrottle::makeRoom(std::chrono::steady_clock::time_point now)
{
	if (_windows.size() < MAX_KEYS) return;

	const auto itFirst = _windows.find(_order.front());
	poco_assert (itFirst != _windows.end());
	if (itFirst->second.end > now && itFirst->second.counted > 0 && now >= _nextTableWarning)
	{
		_nextTableWarning = now + TABLE_WARNING_INTERVAL;
		_logger.warning("The limit of password checks holds the maximum of " + Poco::NumberFormatter::format(MAX_KEYS) +
			" client addresses; the window of " + itFirst->first + ", which had not ended, was removed, so the checks "
			"counted for it are lost. Further removals are not logged for " +
			Poco::NumberFormatter::format(std::chrono::seconds(TABLE_WARNING_INTERVAL).count()) + " s.");
	}
	_windows.erase(itFirst);
	_order.pop_front();
}


void LoginThrottle::refund(const std::string& key, std::uint64_t windowId)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	const auto it = _windows.find(key);
	if (it != _windows.end() && it->second.id == windowId && it->second.counted > 0)
	{
		--it->second.counted;
	}
}


} // namespace Poco::OSP::Auth
