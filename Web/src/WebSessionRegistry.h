//
// WebSessionRegistry.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebSessionRegistry
//
// Definition of the WebSessionRegistry class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_WebSessionRegistry_INCLUDED
#define OSP_Web_WebSessionRegistry_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/Mutex.h"
#include <set>


namespace Poco::OSP::Web {


class WebSession;


class WebSessionRegistry
	/// The sessions created or loaded by a WebSessionManager that still exist,
	/// whether or not the manager still caches them.
	///
	/// The manager and each of these sessions share the registry, so that it
	/// exists as long as any of them does. A session removes itself from the
	/// registry when it is destroyed.
{
public:
	WebSessionRegistry() = default;
		/// Creates an empty registry.

	~WebSessionRegistry() = default;
		/// Destroys the registry. No session refers to it any more.

	WebSessionRegistry(const WebSessionRegistry&) = delete;
	WebSessionRegistry(WebSessionRegistry&&) = delete;
	WebSessionRegistry& operator = (const WebSessionRegistry&) = delete;
	WebSessionRegistry& operator = (WebSessionRegistry&&) = delete;

	void add(WebSession* pSession);
		/// Adds the session.

	void remove(WebSession* pSession);
		/// Removes the session. Waits while forEach() runs.

	template <typename Fn>
	void forEach(Fn&& fn);
		/// Calls fn with every session, while holding the lock, so that
		/// no session is destroyed meanwhile. fn must not destroy a session.

private:
	Poco::FastMutex _mutex;
	std::set<WebSession*> _sessions;
};


//
// inlines
//
inline void WebSessionRegistry::add(WebSession* pSession)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_sessions.insert(pSession);
}


inline void WebSessionRegistry::remove(WebSession* pSession)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_sessions.erase(pSession);
}


template <typename Fn>
void WebSessionRegistry::forEach(Fn&& fn)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	for (WebSession* pSession: _sessions)
	{
		fn(*pSession);
	}
}


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebSessionRegistry_INCLUDED
