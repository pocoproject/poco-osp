//
// TestCase.cpp
//
// Library: OSP/Unit
// Package: Unit
// Module:  TestCase
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Unit/TestCase.h"


namespace Poco::OSP::Unit {


Poco::OSP::BundleContext::Ptr TestCase::_pContext;


TestCase::TestCase(const std::string& name): 
	CppUnit::TestCase(name)
{
}

	
TestCase::~TestCase() = default;


Poco::OSP::BundleContext::Ptr TestCase::context()
{
	return _pContext;
}


void TestCase::resetContext(Poco::OSP::BundleContext::Ptr pContext)
{
	_pContext = pContext;
}


} // namespace Poco::OSP::Unit
