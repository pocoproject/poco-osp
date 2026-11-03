//
// ServiceListenerTest.h
//
// Definition of the ServiceListenerTest class.
//
// Copyright (c) 2007-2018, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef ServiceListenerTest_INCLUDED
#define ServiceListenerTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include <set>


class ServiceListenerTest: public CppUnit::TestCase
{
public:
	ServiceListenerTest(const std::string& name);
	~ServiceListenerTest() override;

	void testListenerReportsMatchingServices();
	void testFailedConstructionLeavesNoDelegates();
	void testServiceUnregisteredDuringCreationIsReportedAsUnregistered();
	void testDestroyedListenerReportsNothing();
	void testListenerWithInvalidQueryThrows();
	void testPropertyChangeIsNotReported();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

protected:
	void handleRegistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef);
	void handleUnregistered(const Poco::OSP::ServiceRef::Ptr& pServiceRef);
	[[noreturn]] void handleRegisteredThrows(const Poco::OSP::ServiceRef::Ptr& pServiceRef);
	void handleRegisteredUnregistersService2(const Poco::OSP::ServiceRef::Ptr& pServiceRef);

private:
	std::set<Poco::OSP::ServiceRef::Ptr> _refs;
	Poco::OSP::ServiceRegistry* _pRegistry = nullptr;
};


#endif // ServiceListenerTest_INCLUDED
