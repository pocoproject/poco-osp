//
// SessionStoreStub.h
//
// Definition of the SessionStoreStub class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SessionStoreStub_INCLUDED
#define SessionStoreStub_INCLUDED


#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionStore.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Any.h"
#include "Poco/AutoPtr.h"
#include "Poco/Mutex.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "Poco/Types.h"
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>


class SessionStoreStub: public Poco::OSP::Web::WebSessionStore
	/// Keeps sessions in memory. loadSession() returns a new WebSession object
	/// for every load, as a store that serializes the sessions does.
	///
	/// A session that has not been saved starts with version 1, like a new
	/// WebSession, when one of its values or its timeout is saved.
{
public:
	using Ptr = Poco::AutoPtr<SessionStoreStub>;

	SessionStoreStub() = default;
		/// Creates an empty store.

	SessionStoreStub(const SessionStoreStub&) = delete;
	SessionStoreStub(SessionStoreStub&&) = delete;
	SessionStoreStub& operator = (const SessionStoreStub&) = delete;
	SessionStoreStub& operator = (SessionStoreStub&&) = delete;

	[[nodiscard]] Poco::OSP::Web::WebSession::Attributes attributes(const std::string& sessionId) const;
		/// Returns the stored attributes of the given session.

	// WebSessionStore
	void saveSession(Poco::OSP::Web::WebSession::Ptr pSession) override;
	[[nodiscard]] std::pair<Poco::OSP::Web::WebSession::Ptr, bool> loadSession(Poco::OSP::BundleContext::Ptr pContext, const std::string& id, Poco::Int64 version) override;
	void expireSession(const std::string& sessionId, Poco::Timespan timeout) override;
	Poco::Int64 saveTimeout(const std::string& sessionId, Poco::Timespan timeout) override;
	Poco::Int64 saveValue(const std::string& sessionId, const std::string& key, const Poco::Any& value) override;
	Poco::Int64 removeValue(const std::string& sessionId, const std::string& key) override;
	Poco::Int64 clearValues(const std::string& sessionId) override;
	[[nodiscard]] std::vector<std::string> sessionsByValue(const std::string& key, const Poco::Any& value) override;
	[[nodiscard]] std::size_t countSessions() override;

protected:
	~SessionStoreStub() override = default;
		/// Reference counted: destroyed through release() only.

private:
	struct StoredSession
	{
		Poco::Int64 version = 1;
		int timeout = 0;
		Poco::Net::IPAddress clientAddress;
		Poco::Timestamp created;
		Poco::OSP::Web::WebSession::Attributes attrs;
	};

	mutable Poco::FastMutex _mutex;
	std::map<std::string, StoredSession> _sessions;
};


#endif // SessionStoreStub_INCLUDED
