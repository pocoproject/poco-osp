//
// BundlesCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  BundlesCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundlesCommand.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Format.h"


namespace Poco::OSP::Shell {


BundlesCommand::BundlesCommand(Poco::OSP::BundleContext::Ptr pContext):
	OSPCommand(pContext, "bundles")
{
}


BundlesCommand::~BundlesCommand() = default;


int BundlesCommand::executeImpl(Session& /*session*/, const std::vector<std::string>& args, std::ostream& ostr)
{
	std::vector<Poco::OSP::Bundle::Ptr> bundles;
	ostr << "    ID  State         Bundle\n";
	context()->listBundles(bundles);
	for (std::vector<Poco::OSP::Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if (match(args, (*it)->symbolicName()))
		{
			ostr << Poco::NumberFormatter::format((*it)->id(), 6) << "  ";
			ostr << Poco::format("%-13s ", (*it)->stateString());
			ostr << (*it)->symbolicName() << "/" << (*it)->version().toString() << '\n';
		}
	}
	
	return Command::STATUS_OK;
}


} // namespace Poco::OSP::Shell
