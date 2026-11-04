//
// SessionStoreStub.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SessionStoreStub.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionStore.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Mutex.h"
#include "Poco/Types.h"
#include "Poco/Timespan.h"
#include "Poco/Any.h"
#include <string>
#include <utility>
#include <vector>
#include <cstddef>


using Poco::OSP::Web::WebSession;
using Poco::OSP::Web::WebSessionStore;


WebSession::Attributes SessionStoreStub::attributes(const std::string& sessionId) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	const auto it = _sessions.find(sessionId);
	return it == _sessions.end() ? WebSession::Attributes() : it->second.attrs;
}


void SessionStoreStub::saveSession(WebSession::Ptr pSession)
{
	// The session is read before the store is locked: the session
	// calls the store while it holds its own lock.
	StoredSession session;
	session.version = pSession->version();
	session.timeout = pSession->timeout();
	session.clientAddress = pSession->clientAddress();
	session.created = pSession->created();
	session.attrs = pSession->attributes();

	Poco::FastMutex::ScopedLock lock(_mutex);

	_sessions[pSession->id()] = session;
}


std::pair<WebSession::Ptr, bool> SessionStoreStub::loadSession(Poco::OSP::BundleContext::Ptr pContext, const std::string& id, Poco::Int64 version)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	const auto it = _sessions.find(id);
	if (it == _sessions.end()) return {WebSession::Ptr(), false};
	if (it->second.version <= version) return {WebSession::Ptr(), true};
	const StoredSession& stored = it->second;
	return {new WebSession(id, stored.version, stored.timeout, stored.clientAddress, WebSessionStore::Ptr(this, true), pContext, stored.created, stored.attrs), true};
}


void SessionStoreStub::expireSession(const std::string& sessionId, Poco::Timespan timeout)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (timeout == 0) _sessions.erase(sessionId);
}


Poco::Int64 SessionStoreStub::saveTimeout(const std::string& sessionId, Poco::Timespan timeout)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	StoredSession& stored = _sessions[sessionId];
	stored.timeout = static_cast<int>(timeout.totalSeconds());
	return ++stored.version;
}


Poco::Int64 SessionStoreStub::saveValue(const std::string& sessionId, const std::string& key, const Poco::Any& value)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	StoredSession& stored = _sessions[sessionId];
	stored.attrs[key] = value;
	return ++stored.version;
}


Poco::Int64 SessionStoreStub::removeValue(const std::string& sessionId, const std::string& key)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	StoredSession& stored = _sessions[sessionId];
	stored.attrs.erase(key);
	return ++stored.version;
}


Poco::Int64 SessionStoreStub::clearValues(const std::string& sessionId)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	StoredSession& stored = _sessions[sessionId];
	stored.attrs.clear();
	return ++stored.version;
}


std::vector<std::string> SessionStoreStub::sessionsByValue(const std::string& key, const Poco::Any& value)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	std::vector<std::string> ids;
	const std::string wanted = Poco::AnyCast<std::string>(value);
	for (const auto& session: _sessions)
	{
		const auto it = session.second.attrs.find(key);
		if (it != session.second.attrs.end() && Poco::AnyCast<std::string>(it->second) == wanted) ids.push_back(session.first);
	}
	return ids;
}


std::size_t SessionStoreStub::countSessions()
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _sessions.size();
}
