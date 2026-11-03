//
// TestBundle.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Exception.h"
#include <atomic>
#include <string>
#include <typeinfo>


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;


namespace
{
	[[nodiscard]] std::atomic<int>& initializationCount()
		/// The initializations of the library by a class loader
		/// that no uninitialization has followed yet.
	{
		static std::atomic<int> count(0);
		return count;
	}
}


class ActivatedService: public Service
	/// The service that MyBundleActivator registers while the
	/// bundle is active, under the symbolic name of the bundle
	/// followed by ".activated".
{
public:
	ActivatedService() = default;

	[[nodiscard]] const std::type_info& type() const override
	{
		return typeid(ActivatedService);
	}

	[[nodiscard]] bool isA(const std::type_info& otherType) const override
	{
		std::string name(typeid(ActivatedService).name());
		return name == otherType.name() || Service::isA(otherType);
	}

protected:
	~ActivatedService() override = default;
};


class MyBundleActivator: public BundleActivator
{
public:
	MyBundleActivator() = default;
	
	~MyBundleActivator() override = default;
	
	void start(BundleContext::Ptr pContext) override
	{
		std::string msg("MyBundleActivator::start() for ");
		msg.append(pContext->thisBundle()->name());
		pContext->logger().information(msg);
		_pServiceRef = pContext->registry().registerService(pContext->thisBundle()->symbolicName() + ".activated", new ActivatedService, Poco::OSP::Properties());
	}
		
	void stop(BundleContext::Ptr pContext) override
	{
		std::string msg("MyBundleActivator::stop() for ");
		msg.append(pContext->thisBundle()->name());
		pContext->logger().information(msg);
		pContext->registry().unregisterService(_pServiceRef);
		_pServiceRef = nullptr;
	}

private:
	ServiceRef::Ptr _pServiceRef;
};


class FailingBundleActivator: public BundleActivator
	/// An activator whose start() throws.
{
public:
	FailingBundleActivator() = default;
	~FailingBundleActivator() override = default;

	void start(BundleContext::Ptr pContext) override
	{
		throw Poco::IllegalStateException("The activator cannot be started", pContext->thisBundle()->symbolicName());
	}

	void stop(BundleContext::Ptr /*pContext*/) override
	{
	}
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(MyBundleActivator)
	POCO_EXPORT_CLASS(FailingBundleActivator)
POCO_END_MANIFEST


void pocoInitializeLibrary()
{
	++initializationCount();
}


void pocoUninitializeLibrary()
{
	--initializationCount();
}


extern "C" int POCO_LIBRARY_API testBundleInitializationCount()
	/// Lets a test that loads the library itself, which does not
	/// initialize it, see whether a class loader still holds it.
{
	return initializationCount();
}
