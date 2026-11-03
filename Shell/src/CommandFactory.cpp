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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
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
