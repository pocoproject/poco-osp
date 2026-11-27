//
// WebSession.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebSession
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionStore.h"
#include "WebSessionRegistry.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/Timespan.h"
#include "Poco/Exception.h"
#include "Poco/Any.h"
#include "Poco/DigestEngine.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>


namespace Poco::OSP::Web {


const std::string WebSession::CSRF_TOKEN("#csrfToken");


WebSession::WebSession(const std::string& id, const std::string& csrfToken, Poco::Int64 version, int timeoutSeconds, const Poco::Net::IPAddress& clientAddress, Poco::AutoPtr<WebSessionStore> pStore, BundleContext::Ptr pContext):
	_id(id),
	_version(version),
	_timeout(timeoutSeconds, 0),
	_pContext(pContext),
	_clientAddress(clientAddress),
	_pStore(pStore)
{
	accessImpl();
	_attrs[CSRF_TOKEN] = csrfToken;
}


WebSession::WebSession(const std::string& id, Poco::Int64 version, int timeoutSeconds, const Poco::Net::IPAddress& clientAddress, Poco::AutoPtr<WebSessionStore> pStore, BundleContext::Ptr pContext, Poco::Timestamp created, const Attributes& attrs):
	_id(id),
	_version(version),
	_timeout(timeoutSeconds, 0),
	_pContext(pContext),
	_created(created),
	_clientAddress(clientAddress),
	_attrs(attrs),
	_pStore(pStore)
{
	accessImpl();
}


WebSession::~WebSession()
{
	try
	{
		if (_pRegistry != nullptr) _pRegistry->remove(this);
		const WebSession* pThis(this);
		sessionEnds.notify(this, pThis);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


bool WebSession::has(const std::string& key) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _attrs.contains(key);
}


Poco::Any WebSession::get(const std::string& key) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	Attributes::const_iterator it = _attrs.find(key);
	if (it != _attrs.end())
		return it->second;
	else
		throw Poco::NotFoundException(key);
}


void WebSession::set(const std::string& key, const Poco::Any& value)
{
	// Declared before the lock, so that the replaced value is destroyed after the
	// lock is released: it may hold the last reference to another session, whose
	// destructor locks the session registry.
	Poco::Any replaced;
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Any& attr = _attrs[key];
	replaced.swap(attr);
	attr = value;

	if (_pStore != nullptr)
	{
		updateVersion(_pStore->saveValue(_id, key, value));
	}
}


void WebSession::erase(const std::string& key)
{
	// Destroyed after the lock is released (see set()).
	Poco::Any removed;
	Poco::FastMutex::ScopedLock lock(_mutex);

	auto it = _attrs.find(key);
	if (it != _attrs.end())
	{
		removed.swap(it->second);
		_attrs.erase(it);
	}

	if (_pStore != nullptr)
	{
		updateVersion(_pStore->removeValue(_id, key));
	}
}


WebSession::Attributes WebSession::attributes() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _attrs;
}


void WebSession::clear()
{
	// Destroyed after the lock is released (see set()).
	Attributes removed;
	Poco::FastMutex::ScopedLock lock(_mutex);

	Poco::Any token;
	auto it = _attrs.find(CSRF_TOKEN);
	if (it != _attrs.end())
	{
		token = it->second;
	}

	removed.swap(_attrs);
	clearImpl();

	if (!token.empty())
	{
		_attrs[CSRF_TOKEN] = token;
		if (_pStore != nullptr)
		{
			updateVersion(_pStore->saveValue(_id, CSRF_TOKEN, token));
		}
	}
}


bool WebSession::isCSRFToken(const std::string& token) const
{
	const std::string expected = getValue<std::string>(CSRF_TOKEN, std::string());
	return !expected.empty() && Poco::DigestEngine::constantTimeEquals(
		Poco::DigestEngine::Digest(token.begin(), token.end()),
		Poco::DigestEngine::Digest(expected.begin(), expected.end()));
}


void WebSession::clearImpl()
{
	_attrs.clear();

	if (_pStore != nullptr)
	{
		updateVersion(_pStore->clearValues(_id));
	}
}


void WebSession::timeout(int seconds)
{
	Poco::Timespan tsTimeout;
	{
		Poco::FastMutex::ScopedLock lock(_timeoutMutex);

		_timeout.assign(seconds, 0);
		accessImpl();
		tsTimeout = _timeout;
	}

	Poco::FastMutex::ScopedLock lock(_mutex);
	if (_pStore != nullptr)
	{
		updateVersion(_pStore->saveTimeout(_id, tsTimeout));
		_pStore->expireSession(_id, tsTimeout);
	}
}


void WebSession::access()
{
	Poco::Timespan tsTimeout;
	{
		Poco::FastMutex::ScopedLock lock(_timeoutMutex);

		accessImpl();
		tsTimeout = _timeout;
	}

	Poco::FastMutex::ScopedLock lock(_mutex);
	if (_pStore != nullptr)
	{
		_pStore->expireSession(_id, tsTimeout);
	}
}


void WebSession::accessImpl()
{
	_expiration.update();
	_expiration += _timeout.totalMicroseconds();
}


void WebSession::updateVersion(Poco::Int64 version)
{
	if (version == _version + 1)
	{
		_version = version;
	}
}


void WebSession::registerWith(const std::shared_ptr<WebSessionRegistry>& pRegistry)
{
	if (_pRegistry == nullptr)
	{
		_pRegistry = pRegistry;
		_pRegistry->add(this);
	}
}


WebSession::Attributes WebSession::releaseAttributes(const Bundle::ConstPtr& pBundle)
{
	Attributes attrs;
	if (pBundle == _pContext->thisBundle())
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		// Only clear attributes in memory, do not clear attributes in session store.
		attrs.swap(_attrs);
	}
	return attrs;
}


void WebSession::retain(std::shared_ptr<const void> pObject)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_retained.push_back(std::move(pObject));
}


void WebSession::retainAll(const WebSession& session)
{
	std::vector<std::shared_ptr<const void>> retained;
	{
		Poco::FastMutex::ScopedLock lock(session._mutex);

		retained = session._retained;
	}
	Poco::FastMutex::ScopedLock lock(_mutex);

	_retained.insert(_retained.end(), retained.begin(), retained.end());
}


bool WebSession::retains(const std::shared_ptr<const void>& pObject) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return std::find(_retained.begin(), _retained.end(), pObject) != _retained.end();
}


} // namespace Poco::OSP::Web
