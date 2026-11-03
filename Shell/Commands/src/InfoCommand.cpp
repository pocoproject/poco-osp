//
// InfoCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  InfoCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "InfoCommand.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


using Poco::OSP::BundleManifest;


namespace Poco::OSP::Shell {


InfoCommand::InfoCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "info")
{
}


InfoCommand::~InfoCommand() = default;


int InfoCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	if (_dependencies)
	{
		doDependencies(pBundle, ostr);
	}
	else if (_clients)
	{
		doClients(pBundle, ostr);
	}
	else
	{
		doInfo(pBundle, ostr);
	}
	return Command::STATUS_OK;
}


void InfoCommand::doInfo(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	ostr << "ID:            " << pBundle->id() << "\n";
	ostr << "Symbolic Name: " << pBundle->symbolicName() << "\n";
	ostr << "Name:          " << pBundle->name() << "\n";
	ostr << "Version:       " << pBundle->version().toString() << "\n";
	ostr << "Vendor:        " << pBundle->vendor() << "\n";
	ostr << "Copyright:     " << pBundle->copyright() << "\n";
	ostr << "State:         " << pBundle->stateString() << "\n";
	ostr << "Run Level:     " << pBundle->runLevel() << "\n";
	ostr << "Path:          " << pBundle->path() << "\n";
}


void InfoCommand::doDependencies(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	const Poco::OSP::BundleManifest::Dependencies& deps = pBundle->requiredBundles();
	for (Poco::OSP::BundleManifest::Dependencies::const_iterator it = deps.begin(); it != deps.end(); ++it)
	{
		ostr << it->symbolicName << " " << it->versions.toString() << "\n";
	}
}


void InfoCommand::doClients(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	std::vector<Bundle::Ptr> bundles;
	context()->listBundles(bundles);
	for (std::vector<Bundle::Ptr>::const_iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		const BundleManifest::Dependencies& deps = (*it)->requiredBundles();
		for (BundleManifest::Dependencies::const_iterator itDep = deps.begin(); itDep != deps.end(); ++itDep)
		{
			if (itDep->symbolicName == pBundle->symbolicName())
			{
				ostr << (*it)->symbolicName() << "/" << (*it)->version().toString() << "\n";
			}
		}
	}
}


void InfoCommand::defineOptions(Poco::Util::OptionSet& options)
{
	OSPCommand::defineOptions(options);
	
	options.addOption(
		Poco::Util::Option("dependencies", "d", "display dependencies of bundle")
			.required(false)
			.repeatable(false)
			.group("deps")
			.callback(Poco::Util::OptionCallback<InfoCommand>(this, &InfoCommand::handleDependencies)));

	options.addOption(
		Poco::Util::Option("clients", "c", "display all bundles that depend on this bundle")
			.required(false)
			.repeatable(false)
			.group("deps")
			.callback(Poco::Util::OptionCallback<InfoCommand>(this, &InfoCommand::handleClients)));
}


void InfoCommand::handleDependencies(const std::string& /*name*/, const std::string& /*value*/)
{
	_dependencies = true;
}


void InfoCommand::handleClients(const std::string& /*name*/, const std::string& /*value*/)
{
	_clients = true;
}


} // namespace Poco::OSP::Shell
