//
// StartCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  StartCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "StartCommand.h"


namespace Poco {
namespace OSP {
namespace Shell {


StartCommand::StartCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "start")
{
}


StartCommand::~StartCommand()
{
}


void StartCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream&)
{
	pBundle->start();
}


} } } // namespace Poco::OSP::Shell
