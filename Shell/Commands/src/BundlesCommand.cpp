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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "BundlesCommand.h"
#include "Poco/NumberFormatter.h"


namespace Poco {
namespace OSP {
namespace Shell {


BundlesCommand::BundlesCommand(Poco::OSP::BundleContext::Ptr pContext):
	OSPCommand(pContext, "bundles")
{
}


BundlesCommand::~BundlesCommand()
{
}


int BundlesCommand::executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	std::vector<Poco::OSP::Bundle::Ptr> bundles;
	ostr << "    ID  State         Bundle\n";
	context()->listBundles(bundles);
	for (std::vector<Poco::OSP::Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if (match(args, (*it)->symbolicName()))
		{
			ostr << Poco::NumberFormatter::format((*it)->id(), 6) << "  ";
			const std::string& state = (*it)->stateString();
			ostr << state << std::string(14 - state.size(), ' ');
			ostr << (*it)->symbolicName() << "/" << (*it)->version().toString() << '\n';
		}
	}
	
	return Command::STATUS_OK;
}


} } } // namespace Poco::OSP::Shell
