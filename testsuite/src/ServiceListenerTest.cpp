//
// ServiceListenerTest.cpp
//
// Copyright (c) 2007-2018, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "ServiceListenerTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceListener.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include <set>


using Poco::OSP::ServiceRegistry;
using Poco::OSP::ServiceListener;
using Poco::OSP::ServiceRef;
using Poco::OSP::Service;
using Poco::OSP::Properties;
using Poco::delegate;


namespace
{
	class TestService: public Service
	{
	public:
		using Ptr = Poco::AutoPtr<TestService>;

		[[nodiscard]] const std::type_info& type() const override
		{
			return typeid(TestService);
		}

		[[nodiscard]] bool isA(const std::type_info& otherType) const override
		{
			std::string name(typeid(TestService).name());
			return name == otherType.name() || Service::isA(otherType);
		}
	};
}


ServiceListenerTest::ServiceListenerTest(const std::string& name): CppUnit::TestCase(name)
{
}


ServiceListenerTest::~ServiceListenerTest() = default;


void ServiceListenerTest::testListenerReportsMatchingServices()
{
	ServiceRegistry reg;
	ServiceListener::Ptr pListener1 = reg.createListener("name == \"Service1\"",
		delegate(this, &ServiceListenerTest::handleRegistered),
		delegate(this, &ServiceListenerTest::handleUnregistered));

	Service::Ptr pService1 = new TestService;
	ServiceRef::Ptr pServiceRef1 = reg.registerService("Service1", pService1, Properties());

	assert (_refs.size() == 1);
	assert (_refs.find(pServiceRef1) != _refs.end());

	Service::Ptr pService2 = new TestService;
	ServiceRef::Ptr pServiceRef2 = reg.registerService("Service2", pService2, Properties());

	assert (_refs.size() == 1);

	reg.unregisterService(pServiceRef1);

	assert (_refs.size() == 0);

	ServiceListener::Ptr pListener2 = reg.createListener("name == \"Service2\"",
		delegate(this, &ServiceListenerTest::handleRegistered),
		delegate(this, &ServiceListenerTest::handleUnregistered));

	assert (_refs.size() == 1);
	assert (_refs.find(pServiceRef2) != _refs.end());

	reg.unregisterService(pServiceRef2);

	assert (_refs.size() == 0);
}


void ServiceListenerTest::testFailedConstructionLeavesNoDelegates()
{
	ServiceRegistry reg;
	Service::Ptr pService1 = new TestService;
	reg.registerService("Service1", pService1, Properties());

	try
	{
		ServiceListener::Ptr pListener = reg.createListener("name == \"Service1\"",
			delegate(this, &ServiceListenerTest::handleRegisteredThrows),
			delegate(this, &ServiceListenerTest::handleUnregistered));
		fail("delegate throws for the registered service - must throw");
	}
	catch (Poco::IllegalStateException&)
	{
	}

	assertTrue (reg.serviceRegistered.empty());
	assertTrue (reg.serviceUnregistered.empty());

	Service::Ptr pService2 = new TestService;
	reg.registerService("Service2", pService2, Properties());
	reg.unregisterService("Service1");
}


void ServiceListenerTest::testServiceUnregisteredDuringCreationIsReportedAsUnregistered()
{
	ServiceRegistry reg;
	Service::Ptr pService1 = new TestService;
	ServiceRef::Ptr pServiceRef1 = reg.registerService("Service1", pService1, Properties());
	Service::Ptr pService2 = new TestService;
	reg.registerService("Service2", pService2, Properties());

	// Service1 is reported first. Its delegate unregisters Service2, which
	// the listener has found, but not reported yet.
	_pRegistry = &reg;
	ServiceListener::Ptr pListener = reg.createListener("name =~ \"Service*\"",
		delegate(this, &ServiceListenerTest::handleRegisteredUnregistersService2),
		delegate(this, &ServiceListenerTest::handleUnregistered));
	_pRegistry = nullptr;

	assertTrue (_refs == std::set<ServiceRef::Ptr>({pServiceRef1}));
}


void ServiceListenerTest::testDestroyedListenerReportsNothing()
{
	ServiceRegistry reg;
	ServiceListener::Ptr pListener = reg.createListener("name == \"Service1\"",
		delegate(this, &ServiceListenerTest::handleRegistered),
		delegate(this, &ServiceListenerTest::handleUnregistered));
	pListener = nullptr;

	reg.registerService("Service1", new TestService, Properties());
	assertTrue (_refs.empty());
	assertTrue (reg.serviceRegistered.empty());
	assertTrue (reg.serviceUnregistered.empty());
}


void ServiceListenerTest::testListenerWithInvalidQueryThrows()
{
	ServiceRegistry reg;
	try
	{
		ServiceListener::Ptr pListener = reg.createListener("name ==",
			delegate(this, &ServiceListenerTest::handleRegistered),
			delegate(this, &ServiceListenerTest::handleUnregistered));
		fail("invalid query - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
	assertTrue (reg.serviceRegistered.empty());
	assertTrue (reg.serviceUnregistered.empty());
}


void ServiceListenerTest::testPropertyChangeIsNotReported()
{
	ServiceRegistry reg;
	Properties props;
	props.set("foo", "baz");
	ServiceRef::Ptr pServiceRef = reg.registerService("Service1", new TestService, props);
	ServiceListener::Ptr pListener = reg.createListener("foo == \"bar\"",
		delegate(this, &ServiceListenerTest::handleRegistered),
		delegate(this, &ServiceListenerTest::handleUnregistered));
	assertTrue (_refs.empty());

	// a listener evaluates its query when a service is registered
	pServiceRef->properties().set("foo", "bar");
	assertTrue (_refs.empty());
}


void ServiceListenerTest::handleRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
{
	_refs.insert(pServiceRef);
}


void ServiceListenerTest::handleUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
{
	_refs.erase(pServiceRef);
}


void ServiceListenerTest::handleRegisteredThrows(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
{
	throw Poco::IllegalStateException("delegate fails", pServiceRef->name());
}


void ServiceListenerTest::handleRegisteredUnregistersService2(const Poco::OSP::ServiceRef::Ptr& pServiceRef)
{
	_refs.insert(pServiceRef);
	if (pServiceRef->name() == "Service1") _pRegistry->unregisterService("Service2");
}


void ServiceListenerTest::setUp()
{
	_refs.clear();
}


void ServiceListenerTest::tearDown()
{
}


CppUnit::Test* ServiceListenerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ServiceListenerTest");

	CppUnit_addTest(pSuite, ServiceListenerTest, testListenerReportsMatchingServices);
	CppUnit_addTest(pSuite, ServiceListenerTest, testFailedConstructionLeavesNoDelegates);
	CppUnit_addTest(pSuite, ServiceListenerTest, testServiceUnregisteredDuringCreationIsReportedAsUnregistered);
	CppUnit_addTest(pSuite, ServiceListenerTest, testDestroyedListenerReportsNothing);
	CppUnit_addTest(pSuite, ServiceListenerTest, testListenerWithInvalidQueryThrows);
	CppUnit_addTest(pSuite, ServiceListenerTest, testPropertyChangeIsNotReported);

	return pSuite;
}
