//
// ServiceListener.cpp
//
// Library: OSP
// Package: Service
// Module:  ServiceListener
//
// Copyright (c) 2018, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/ServiceListener.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceEvent.h"
#include "Poco/OSP/QLExpr.h"
#include "Poco/OSP/QLParser.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Bugcheck.h"
#include <string>


using namespace std::string_literals;


namespace Poco::OSP {


ServiceListener::~ServiceListener()
{
	try
	{
		_registry.serviceRegistered   -= Poco::delegate(this, &ServiceListener::onServiceRegistered);
		_registry.serviceUnregistered -= Poco::delegate(this, &ServiceListener::onServiceUnregistered);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void ServiceListener::init(const std::string& query)
{
	QLParser parser(query);
	_pExpr = parser.parse();

	// Subscribed before the existing services are enumerated, so that no
	// registration is missed. A service that is unregistered after find()
	// and before it has been reported is reported as unregistered after it.
	_registry.serviceRegistered   += Poco::delegate(this, &ServiceListener::onServiceRegistered);
	_registry.serviceUnregistered += Poco::delegate(this, &ServiceListener::onServiceUnregistered);

	// If a delegate or the query throws, the constructor fails and the
	// object is freed, so the registry must not keep the delegates.
	try
	{
		std::vector<ServiceRef::Ptr> results;
		if (_registry.find(*_pExpr, results))
		{
			for (std::vector<ServiceRef::Ptr>::const_iterator it = results.begin(); it != results.end(); ++it)
			{
				onServiceRefRegistered(*it);
				bool unregistered = false;
				{
					Poco::FastMutex::ScopedLock lock(_mutex);
					unregistered = _unregisteredWhileInitializing.contains(*it);
				}
				if (unregistered)
				{
					onServiceRefUnregistered(*it);
				}
			}
		}
	}
	catch (...)
	{
		_registry.serviceRegistered   -= Poco::delegate(this, &ServiceListener::onServiceRegistered);
		_registry.serviceUnregistered -= Poco::delegate(this, &ServiceListener::onServiceUnregistered);
		throw;
	}

	Poco::FastMutex::ScopedLock lock(_mutex);
	_initializing = false;
	_unregisteredWhileInitializing.clear();
}


void ServiceListener::onServiceRegistered(ServiceEvent& event)
{
	onServiceRefRegistered(event.service());
}


void ServiceListener::onServiceUnregistered(ServiceEvent& event)
{
	onServiceRefUnregistered(event.service());
}


void ServiceListener::onServiceRefRegistered(const ServiceRef::Ptr& pServiceRef)
{
	// An evaluation error must not fail the registration of the service.
	bool matches = false;
	try
	{
		matches = _pExpr->evaluate(pServiceRef->properties());
	}
	catch (Poco::Exception& exc)
	{
		Poco::Logger::get("osp.core.ServiceRegistry"s).warning(
			"The query of a service listener cannot be evaluated for service %s, "
			"which is not reported to the listener: %s"s,
			pServiceRef->name(), exc.displayText());
	}
	if (matches)
	{
		bool notify = false;
		Poco::ScopedLockWithUnlock<Poco::FastMutex> lock(_mutex);
		if (!_refs.contains(pServiceRef))
		{
			_refs.insert(pServiceRef);
			notify = true;
		}
		lock.unlock();
		if (notify)
		{
			serviceRegistered(this, pServiceRef);
		}
	}
}


void ServiceListener::onServiceRefUnregistered(const ServiceRef::Ptr& pServiceRef)
{
	Poco::ScopedLockWithUnlock<Poco::FastMutex> lock(_mutex);
	std::set<ServiceRef::Ptr>::iterator it = _refs.find(pServiceRef);
	if (it != _refs.end())
	{
		_refs.erase(it);
		lock.unlock();
		serviceUnregistered(this, pServiceRef);
	}
	else if (_initializing)
	{
		_unregisteredWhileInitializing.insert(pServiceRef);
	}
}


} // namespace Poco::OSP
