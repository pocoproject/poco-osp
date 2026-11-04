//
// CommandFactory.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  CommandFactory
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/CommandFactory.h"


namespace Poco::OSP::Shell {


CommandFactory::CommandFactory() = default;

	
CommandFactory::~CommandFactory() = default;


void CommandFactory::init(Poco::OSP::BundleContext::Ptr pContext)
{
	_pContext = pContext;
	initImpl();
}


void CommandFactory::initImpl()
{
}


} // namespace Poco::OSP::Shell
