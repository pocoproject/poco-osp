//
// BundleActionCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  BundleActionCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleActionCommand.h"
#include "Poco/NumberFormatter.h"


namespace Poco::OSP::Shell {


BundleActionCommand::BundleActionCommand(Poco::OSP::BundleContext::Ptr pContext, const std::string& command):
	OSPCommand(pContext, command)
{
}


BundleActionCommand::~BundleActionCommand() = default;


int BundleActionCommand::executeImpl(Session& /*session*/, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.empty())
	{
		ostr << "No bundle specified." << '\n';
		return Command::STATUS_USAGE;
	}
	for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it)
	{
		Poco::OSP::Bundle::Ptr pBundle = findBundle(*it);
		if (pBundle != nullptr)
		{
			const int status = doAction(pBundle, ostr);
			if (status != Command::STATUS_OK) return status;
		}
		else
		{
			ostr << "No such bundle: " << *it << '\n';
			return Command::STATUS_ERROR;
		}
	}
	return Command::STATUS_OK;
}


} // namespace Poco::OSP::Shell
