//
// LoginThrottle.h
//
// Library: OSP
// Package: Auth
// Module:  LoginThrottle
//
// Definition of the LoginThrottle class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_LoginThrottle_INCLUDED
#define OSP_Auth_LoginThrottle_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>


namespace Poco::OSP::Auth {


class OSP_API LoginThrottle
	/// Limits the password checks per client: at most maxAttempts checks of one key (see ClientAddressKey.h) are
	/// counted in a window of cooldown, which starts at the first counted check. A check is counted when it
	/// begins, so that parallel checks cannot exceed the limit, and refunded when it succeeds, unless it was counted
	/// in an earlier window; a success does not clear the checks counted before it. When maxAttempts are counted,
	/// begin() refuses further checks of the key until the window ends.
	///
	/// The table of windows holds at most MAX_KEYS keys. When a new key does not fit, the window that ends first
	/// is removed. If that window has not ended and counts checks, these checks are lost, and a warning is logged
	/// (logger "osp.core.LoginThrottle"), at most once per minute.
	///
	/// LoginThrottle is thread-safe.
{
public:
	using Clock = std::function<std::chrono::steady_clock::time_point()>;
		/// Returns the current time. LoginThrottle calls it with its mutex locked.

	static constexpr int DEFAULT_MAX_ATTEMPTS = 10;
	static constexpr std::chrono::seconds DEFAULT_COOLDOWN{60};
	static constexpr std::chrono::seconds MIN_COOLDOWN{1};
		/// The shortest cooldown that a LoginThrottle accepts.
	static constexpr std::chrono::seconds MAX_COOLDOWN{86400};
		/// The longest cooldown that a LoginThrottle accepts.
	static constexpr std::size_t MAX_KEYS = 16384;

	struct Settings
		/// The settings of a LoginThrottle.
	{
		int maxAttempts = DEFAULT_MAX_ATTEMPTS;
			/// The number of checks counted per key in a window, 0 or greater. 0 turns the limit off.
		std::chrono::seconds cooldown = DEFAULT_COOLDOWN;
			/// The length of a window, from MIN_COOLDOWN to MAX_COOLDOWN while maxAttempts is greater than 0.
	};

	class OSP_API Attempt
		/// A password check that begin() has counted, or a refusal. An Attempt that is destroyed
		/// without succeeded() stays counted. The LoginThrottle must outlive its Attempts.
		///
		/// Attempt is move-only; a moved-from Attempt refunds nothing.
	{
	public:
		Attempt() = delete;
		Attempt(const Attempt&) = delete;
		Attempt(Attempt&& attempt) noexcept;
		~Attempt();
		Attempt& operator = (const Attempt&) = delete;
		Attempt& operator = (Attempt&& attempt) noexcept;

		[[nodiscard]] bool admitted() const;
			/// Returns true if the check may run, false if it must not run.

		[[nodiscard]] bool firstRefusal() const;
			/// Returns true for the first refusal of the key in a window, so that the caller
			/// can log the refusals once per window.

		[[nodiscard]] std::chrono::seconds retryAfter() const;
			/// Returns, for a refusal, the whole seconds until the window ends, at least 1.
			/// Returns 0 for an admitted check.

		[[nodiscard]] const std::string& key() const;
			/// Returns the key of the check.

		void succeeded();
			/// Refunds the check, once. Does nothing for a refusal, for a check that counts
			/// nothing (exempt() or a disabled LoginThrottle), and for a check that was counted
			/// in an earlier window.

	private:
		explicit Attempt(std::string key);
		Attempt(LoginThrottle& throttle, std::string key, std::uint64_t windowId);
		Attempt(std::string key, std::chrono::seconds retryAfter, bool firstRefusal);

		LoginThrottle* _pThrottle = nullptr;
		std::string _key;
		std::uint64_t _windowId = 0;
		std::chrono::seconds _retryAfter{0};
		bool _admitted = true;
		bool _firstRefusal = false;

		friend class LoginThrottle;
	};

	LoginThrottle() = delete;

	explicit LoginThrottle(Settings settings, Clock clock = {});
		/// Creates a LoginThrottle with the given settings. An empty clock
		/// means std::chrono::steady_clock::now().
		///
		/// Throws a Poco::InvalidArgumentException for a negative maxAttempts, and for a cooldown
		/// outside MIN_COOLDOWN to MAX_COOLDOWN while maxAttempts is greater than 0.

	~LoginThrottle();
		/// Destroys the LoginThrottle.

	LoginThrottle(const LoginThrottle&) = delete;
	LoginThrottle(LoginThrottle&&) = delete;
	LoginThrottle& operator = (const LoginThrottle&) = delete;
	LoginThrottle& operator = (LoginThrottle&&) = delete;

	[[nodiscard]] Attempt begin(const std::string& key);
		/// Counts a password check of the given key and returns it as an admitted Attempt,
		/// or returns a refusal if maxAttempts checks of the key are counted in the current window.
		/// A disabled LoginThrottle admits every check and counts nothing.

	[[nodiscard]] static Attempt exempt(const std::string& key);
		/// Returns an admitted Attempt that counts nothing, for a client that the caller
		/// found exempt (see isExemptAddress() in ClientAddressKey.h).

	static void logRefusal(Poco::Logger& logger, const Attempt& refusal, const std::string& userName);
		/// Logs a refused password check of the given user to the given logger: a warning for the first
		/// refusal of the key in a window, and a debug message for every refusal. The user name is escaped
		/// with Poco::UTF8::escape(). The attempt must be a refusal.

	[[nodiscard]] const Settings& settings() const;
		/// Returns the settings.

	[[nodiscard]] bool enabled() const;
		/// Returns true if maxAttempts is greater than 0.

	[[nodiscard]] static bool isValidCooldown(std::chrono::seconds cooldown);
		/// Returns true if the cooldown is from MIN_COOLDOWN to MAX_COOLDOWN.

private:
	using Order = std::list<std::string>;

	static constexpr std::chrono::minutes TABLE_WARNING_INTERVAL{1};
		/// The shortest time between two warnings about windows removed from a full table.

	struct Window
	{
		std::chrono::steady_clock::time_point end{};
		int counted = 0;
		std::uint64_t id = 0;
		bool refused = false;
		Order::iterator itOrder{};
	};

	[[nodiscard]] Window newWindow(std::chrono::steady_clock::time_point now, Order::iterator itOrder);
	void makeRoom(std::chrono::steady_clock::time_point now);
	void refund(const std::string& key, std::uint64_t windowId);

	Settings _settings;
	Clock _clock;
	std::unordered_map<std::string, Window> _windows;
	Order _order;
		/// The keys of _windows in the order in which their windows end: all windows have the same length,
		/// so the key of a window that starts moves to the end.
	std::uint64_t _lastWindow = 0;
	std::chrono::steady_clock::time_point _nextTableWarning = std::chrono::steady_clock::time_point::min();
	Poco::Logger& _logger;
	Poco::FastMutex _mutex;
};


//
// inlines
//
inline bool LoginThrottle::Attempt::admitted() const
{
	return _admitted;
}


inline bool LoginThrottle::Attempt::firstRefusal() const
{
	return _firstRefusal;
}


inline std::chrono::seconds LoginThrottle::Attempt::retryAfter() const
{
	return _retryAfter;
}


inline const std::string& LoginThrottle::Attempt::key() const
{
	return _key;
}


inline const LoginThrottle::Settings& LoginThrottle::settings() const
{
	return _settings;
}


inline bool LoginThrottle::enabled() const
{
	return _settings.maxAttempts > 0;
}


inline bool LoginThrottle::isValidCooldown(std::chrono::seconds cooldown)
{
	return cooldown >= MIN_COOLDOWN && cooldown <= MAX_COOLDOWN;
}


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_LoginThrottle_INCLUDED
