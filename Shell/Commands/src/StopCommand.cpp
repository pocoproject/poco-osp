//
// StopCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  StopCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "StopCommand.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <ostream>


using Poco::OSP::BundleManifest;


namespace Poco {
namespace OSP {
namespace Shell {


StopCommand::StopCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "stop"),
	_force(false),
	_all(false)
{
}


StopCommand::~StopCommand()
{
}


int StopCommand::doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr)
{
	if (isRequiredByShell(pBundle))
	{
		ostr << "Cannot stop bundle " << pBundle->symbolicName() << " from a shell session: the shell requires it.\n";
		return Command::STATUS_ERROR;
	}

	BundleVec deps;
	if (!_force)
	{
		dependingBundles(pBundle, deps);
	}

	if (deps.empty())
	{
		pBundle->stop();
	}
	else if (_all)
	{
		stopAll(pBundle);
	}
	else
	{
		ostr << "Cannot stop bundle " << pBundle->symbolicName() << " as the following bundles depend on it:\n";
		for (BundleVec::const_iterator it = deps.begin(); it != deps.end(); ++it)
		{
			ostr << (*it)->symbolicName() << " (" << (*it)->id() << ")\n";
		}
		ostr << "Use the --all option to stop all depending bundles, or --force to override at your own risk.";
		return Command::STATUS_ERROR;
	}
	return Command::STATUS_OK;
}


void StopCommand::defineOptions(Poco::Util::OptionSet& options)
{
	OSPCommand::defineOptions(options);

	options.addOption(
		Poco::Util::Option("force", "f", "force stopping of bundle even if other started bundles depend on it")
			.required(false)
			.repeatable(false)
			.callback(Poco::Util::OptionCallback<StopCommand>(this, &StopCommand::handleForce)));

	options.addOption(
		Poco::Util::Option("all", "a", "also stop all bundles depending on this bundle")
			.required(false)
			.repeatable(false)
			.callback(Poco::Util::OptionCallback<StopCommand>(this, &StopCommand::handleAll)));
}


void StopCommand::handleForce(const std::string& name, const std::string& value)
{
	_force = true;
}


void StopCommand::handleAll(const std::string& name, const std::string& value)
{
	_all = true;
}


void StopCommand::dependingBundles(Poco::OSP::Bundle::Ptr pBundle, BundleVec& dependingBundles)
{
	BundleVec bundles;
	context()->listBundles(bundles);
	for (BundleVec::const_iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if ((*it)->isStarted())
		{
			const BundleManifest::Dependencies& deps = (*it)->requiredBundles();
			for (BundleManifest::Dependencies::const_iterator itDep = deps.begin(); itDep != deps.end(); ++itDep)
			{
				if (itDep->symbolicName == pBundle->symbolicName())
				{
					dependingBundles.push_back(*it);
					break;
				}
			}
		}
	}
}


void StopCommand::stopAll(Poco::OSP::Bundle::Ptr pBundle)
{
	BundleVec depending;
	dependingBundles(pBundle, depending);
	for (BundleVec::iterator it = depending.begin(); it != depending.end(); ++it)
	{
		if ((*it)->isStarted()) stopAll(*it);
	}
	pBundle->stop();
}


} } } // namespace Poco::OSP::Shell
