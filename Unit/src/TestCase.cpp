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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Unit/TestCase.h"


namespace Poco {
namespace OSP {
namespace Unit {


Poco::OSP::BundleContext::Ptr TestCase::_pContext;


TestCase::TestCase(const std::string& name): 
	CppUnit::TestCase(name)
{
}

	
TestCase::~TestCase()
{
}


Poco::OSP::BundleContext::Ptr TestCase::context()
{
	return _pContext;
}


void TestCase::resetContext(Poco::OSP::BundleContext::Ptr pContext)
{
	_pContext = pContext;
}


} } } // namespace Poco::OSP::Unit
