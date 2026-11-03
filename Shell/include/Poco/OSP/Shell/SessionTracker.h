//
// SessionTracker.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  SessionTracker
//
// Definition of the SessionTracker class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_SessionTracker_INCLUDED
#define OSP_Shell_SessionTracker_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/Net/StreamSocket.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>


namespace Poco::OSP::Shell {


class OSPShell_API SessionTracker
	/// SessionTracker knows the sockets of the active sessions of a shell
	/// server, so that the server can shut them down when it stops and wait
	/// until the sessions have ended.
	///
	/// All member functions can be called from several threads at once.
{
public:
	SessionTracker();
		/// Creates the SessionTracker without sessions.

	~SessionTracker();
		/// Destroys the SessionTracker.

	SessionTracker(const SessionTracker&) = delete;
	SessionTracker(SessionTracker&&) = delete;
	SessionTracker& operator = (const SessionTracker&) = delete;
	SessionTracker& operator = (SessionTracker&&) = delete;

	void add(const Poco::Net::StreamSocket& socket);
		/// Adds the socket of a session that starts. After stop(), the socket
		/// is shut down at once.

	void remove(const Poco::Net::StreamSocket& socket);
		/// Removes the socket of a session that has ended.

	void stop();
		/// Shuts down the sockets of the active sessions, so that they end, and
		/// ends the waits of waitForStop().

	void waitForStop(std::chrono::milliseconds timeout);
		/// Waits until stop() is called, at most for the given time.

	[[nodiscard]] bool stopped() const;
		/// Returns true once stop() has been called.

	[[nodiscard]] std::size_t waitUntilEnded(std::chrono::milliseconds timeout);
		/// Waits until every session has been removed, at most for the given
		/// time, and returns the number of sessions that are still active.

private:
	static void shutdown(Poco::Net::StreamSocket& socket);

	mutable std::mutex _mutex;
	std::condition_variable _changed;
	std::vector<Poco::Net::StreamSocket> _sockets;
	bool _stopped = false;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_SessionTracker_INCLUDED
