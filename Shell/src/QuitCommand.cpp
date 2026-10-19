//
// QuitCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  QuitCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/QuitCommand.h"
#include "Poco/OSP/Shell/Session.h"


namespace Poco {
namespace OSP {
namespace Shell {


QuitCommand::QuitCommand(Poco::OSP::BundleContext::Ptr pContext):
	AbstractCommand(
		pContext->thisBundle()->properties().getString("quit.description"),
		pContext->thisBundle()->properties().getString("quit.usage")),
	_pContext(pContext)
{
}


QuitCommand::~QuitCommand()
{
}


int QuitCommand::executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	return -1;
}


} } } // namespace Poco::OSP::Shell
