//
// SessionTracker.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  SessionTracker
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Exception.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <mutex>


namespace Poco::OSP::Shell {


SessionTracker::SessionTracker() = default;


SessionTracker::~SessionTracker() = default;


void SessionTracker::add(const Poco::Net::StreamSocket& socket)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_sockets.push_back(socket);
	if (_stopped) shutdown(_sockets.back());
}


void SessionTracker::remove(const Poco::Net::StreamSocket& socket)
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		const auto it = std::find(_sockets.begin(), _sockets.end(), socket);
		if (it != _sockets.end()) _sockets.erase(it);
	}
	_changed.notify_all();
}


void SessionTracker::stop()
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_stopped = true;
		for (auto& socket: _sockets)
		{
			shutdown(socket);
		}
	}
	_changed.notify_all();
}


void SessionTracker::waitForStop(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> lock(_mutex);
	_changed.wait_for(lock, timeout, [this]() { return _stopped; });
}


bool SessionTracker::stopped() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _stopped;
}


std::size_t SessionTracker::waitUntilEnded(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> lock(_mutex);
	_changed.wait_for(lock, timeout, [this]() { return _sockets.empty(); });
	return _sockets.size();
}


void SessionTracker::shutdown(Poco::Net::StreamSocket& socket)
{
	try
	{
		socket.shutdown();
	}
	catch (Poco::Exception&)
	{
		// The client has closed the connection already, so the session ends anyway.
	}
}


} // namespace Poco::OSP::Shell
