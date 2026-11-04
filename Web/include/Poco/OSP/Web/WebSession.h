//
// WebSession.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebSession
//
// Definition of the WebSession class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_WebSession_INCLUDED
#define OSP_Web_WebSession_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Timestamp.h"
#include "Poco/Timespan.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Any.h"
#include "Poco/BasicEvent.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"
#include <atomic>
#include <map>
#include <memory>
#include <vector>


namespace Poco::OSP::Web {


class WebSessionStore;
class WebSessionRegistry;


class OSPWeb_API WebSession
	/// A WebSession is used for tracking users between different
	/// HTTP(S) requests. WebSession objects are managed by
	/// a session manager (usually, an implementation of the WebSessionService).
	///
	/// The WebSession can also be used to store arbitrary data
	/// (attributes) in the form of key-value pairs between different
	/// requests. Poco::Any is used for storing values, so practically
	/// any object can be attached to a session.
	///
	/// A WebSession has a time out. If a WebSession instance is not
	/// accessed for a given time, it will be destroyed by the
	/// session manager.
	///
	/// Note that the iterator-based access methods (find(), begin(),
	/// end()) are not thread-safe. While other threads may access
	/// the session, use attributes(), which returns a copy.
	///
	/// When a new session is created, the session manager will generate a
	/// CSRF synchronizer token, which can be used in forms to prevent
	/// Cross-Site Request Forgery (CSRF) attacks. The name of the property
	/// is "#csrfToken". It can also be obtained by calling the csrfToken()
	/// member function.
{
public:
	Poco::BasicEvent<const WebSession*> sessionEnds;
		/// Fired before the session object is destroyed.

	using Attributes = std::map<std::string, Poco::Any>;
	using Ptr = Poco::SharedPtr<WebSession>;

	WebSession(const std::string& id, const std::string& csrfToken, Poco::Int64 version, int timeoutSeconds, const Poco::Net::IPAddress& clientAddress, Poco::AutoPtr<WebSessionStore> pStore, BundleContext::Ptr pContext);
		/// Creates a new WebSession with the given ID and time out.

	WebSession(const std::string& id, Poco::Int64 version, int timeoutSeconds, const Poco::Net::IPAddress& clientAddress, Poco::AutoPtr<WebSessionStore> pStore, BundleContext::Ptr pContext, Poco::Timestamp created, const Attributes& attrs);
		/// Creates a new WebSession with the given ID and time out.

	virtual ~WebSession();
		/// Fires a sessionEnds event and destroys the WebSession.

	[[nodiscard]] const std::string& id() const;
		/// The unique identifier of the session.

	[[nodiscard]] Poco::Int64 version() const;
		/// Returns the version number of the session.

	[[nodiscard]] const Poco::Timestamp& created() const;
		/// Returns the creation time of the session, i.e. the time the user sent the first request.

	[[nodiscard]] bool has(const std::string& key) const;
		/// Returns true iff the session has an attribute with the given value.

	[[nodiscard]] Poco::Any get(const std::string& key) const;
		/// Returns a copy of the attribute with the given key.
		///
		/// Throws a Poco::NotFoundException
		/// if the attribute does not exist.

	template <typename T>
	[[nodiscard]] T getValue(const std::string& key) const
		/// Convenience function that returns the attribute with the given key,
		/// casted to the desired type.
		///
		/// Throws a Poco::NotFoundException if the attribute does not exist.
		/// Throws a Poco::BadCastException if the cast is invalid.
	{
		const Poco::Any& any = get(key);
		return Poco::AnyCast<T>(any);
	}

	template <typename T>
	[[nodiscard]] T getValue(const std::string& key, T deflt) const
		/// Convenience function that returns the attribute with the given key,
		/// casted to the desired type. If the attribute does not exist,
		/// the given default value is returned.
		///
		/// Throws a Poco::BadCastException if the cast is invalid.
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		Attributes::const_iterator it = _attrs.find(key);
		if (it != _attrs.end())
			return Poco::AnyCast<T>(it->second);
		else
			return deflt;
	}

	void set(const std::string& key, const Poco::Any& value);
		/// Sets/Overwrites an attribute value.

	template <typename T>
	void setValue(const std::string& key, T value)
		/// Sets/Overwrites an attribute value.
	{
		set(key, Poco::Any(value));
	}

	void erase(const std::string& key);
		/// Erases an attribute value from the session.

	[[nodiscard]] Attributes::const_iterator find(const std::string& key) const;
		/// Searches for an attribute. Returns end() if not found.

	[[nodiscard]] Attributes::const_iterator begin() const;
		/// Returns the begin iterator for attributes.

	[[nodiscard]] Attributes::const_iterator end() const;
		/// Returns the end iterator for attributes.

	[[nodiscard]] Attributes attributes() const;
		/// Returns a copy of all attributes, taken while holding
		/// the session's lock.

	void clear();
		/// Erases all attributes except the CSRF synchronizer token,
		/// also in the session store.

	[[nodiscard]] int timeout() const;
		/// Returns the timeout of the session in seconds.

	void timeout(int seconds);
		/// Sets a new timeout in seconds for the session.

	[[nodiscard]] const Poco::Net::IPAddress& clientAddress() const;
		/// Returns the IP address of the client holding the session.

	void access();
		/// Updates the expiration time.

	[[nodiscard]] std::string csrfToken() const;
		/// Returns the CSRF synchronizer token for this session.

	[[nodiscard]] bool isCSRFToken(const std::string& token) const;
		/// Returns true if the session has a CSRF synchronizer token equal to the
		/// given one. The comparison takes the same time for every token of the
		/// same length.

	// UniqueExpireCache support
	[[nodiscard]] Poco::Timestamp getExpiration() const;
		/// Return the time when the session will expire.

	static const std::string CSRF_TOKEN;
		/// The name of the attribute storing the CSRF synchronizer token.

protected:
	void registerWith(const std::shared_ptr<WebSessionRegistry>& pRegistry);
		/// Adds the session to the registry of live sessions of the WebSessionManager,
		/// unless it already is in one. The session removes itself when it is destroyed.

	[[nodiscard]] Attributes releaseAttributes(const Bundle::ConstPtr& pBundle);
		/// If the session is owned by the given bundle, removes all attributes
		/// from memory (not from the session store) and returns them.
		/// The WebSessionManager calls this for every live session when a bundle stops.
		/// It is not virtual and uses only members of WebSession, so the manager may call
		/// it while the destructor of the session waits to remove it from the registry.
		///
		/// After the bundle owning the session has been stopped and its libraries have
		/// been unloaded, virtual destructors of objects stored in the session
		/// might no longer be available. Therefore all attributes must be removed
		/// while their object's destructors are still available.

	void retain(std::shared_ptr<const void> pObject);
		/// Keeps pObject until the session has been destroyed, after its attributes.
		/// Like releaseAttributes(), it may be called while the destructor of the session
		/// waits to remove it from the registry.

	void retainAll(const WebSession& session);
		/// Keeps the objects that the given session keeps (see retain()), for a
		/// session that gets the attributes of the given session.

	[[nodiscard]] bool retains(const std::shared_ptr<const void>& pObject) const;
		/// Returns true if the session keeps pObject (see retain()).

	void updateVersion(Poco::Int64 version);
		/// Updates the version if the new version is one higher than the current version.

	void accessImpl();
		/// Updates the expiration time.

	void clearImpl();
		/// Erases all attributes. The caller holds the session's lock; values
		/// that may hold sessions must be moved out before (see clear()), so
		/// that they are destroyed after the lock is released.

private:
	std::string          _id;
	std::atomic<Poco::Int64> _version;
	Poco::Timespan       _timeout;
	BundleContext::Ptr   _pContext;
	Poco::Timestamp      _created;
	Poco::Timestamp      _expiration;
	Poco::Net::IPAddress _clientAddress;
	// Declared before _attrs, so that the attributes are destroyed first: the objects
	// can keep the libraries loaded that the destructors of the attributes are in.
	std::vector<std::shared_ptr<const void>> _retained;
	Attributes           _attrs;
	Poco::AutoPtr<WebSessionStore> _pStore;
	std::shared_ptr<WebSessionRegistry> _pRegistry;

	mutable Poco::FastMutex _mutex;
	mutable Poco::FastMutex _timeoutMutex;

	friend class WebSessionManager;
		// The manager creates, renews and registers the sessions through members that are not public API.
};


//
// inlines
//
inline const std::string& WebSession::id() const
{
	return _id;
}


inline Poco::Int64 WebSession::version() const
{
	return _version.load();
}


inline int WebSession::timeout() const
{
	Poco::FastMutex::ScopedLock lock(_timeoutMutex);

	return _timeout.totalSeconds();
}


inline const Poco::Timestamp& WebSession::created() const
{
	return _created;
}


inline const Poco::Net::IPAddress& WebSession::clientAddress() const
{
	return _clientAddress;
}


inline Poco::Timestamp WebSession::getExpiration() const
{
	Poco::FastMutex::ScopedLock lock(_timeoutMutex);

	return _expiration;
}


inline WebSession::Attributes::const_iterator WebSession::find(const std::string& key) const
{
	return _attrs.find(key);
}


inline WebSession::Attributes::const_iterator WebSession::begin() const
{
	return _attrs.begin();
}


inline WebSession::Attributes::const_iterator WebSession::end() const
{
	return _attrs.end();
}


inline std::string WebSession::csrfToken() const
{
	return getValue<std::string>(CSRF_TOKEN);
}


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebSession_INCLUDED
