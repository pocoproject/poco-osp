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
// SPDX-License-Identifier:	BSL-1.0
//


#include "StopCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Bundle.h"
#include <algorithm>
#include <ostream>


namespace Poco::OSP::Shell {


StopCommand::StopCommand(Poco::OSP::BundleContext::Ptr pContext):
	BundleActionCommand(pContext, "stop")
{
}


StopCommand::~StopCommand() = default;


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
		context()->listRequiringBundles(pBundle, deps);
		const auto itNotStarted = std::remove_if(deps.begin(), deps.end(), [](const Poco::OSP::Bundle::Ptr& pDep)
		{
			return !pDep->isStarted();
		});
		deps.erase(itNotStarted, deps.end());
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


void StopCommand::handleForce(const std::string& /*name*/, const std::string& /*value*/)
{
	_force = true;
}


void StopCommand::handleAll(const std::string& /*name*/, const std::string& /*value*/)
{
	_all = true;
}


void StopCommand::stopAll(Poco::OSP::Bundle::Ptr pBundle)
{
	// In the order of the list, a bundle is stopped before the bundles it requires.
	BundleVec requiring;
	context()->listRequiringBundles(pBundle, requiring);
	for (BundleVec::iterator it = requiring.begin(); it != requiring.end(); ++it)
	{
		if ((*it)->isStarted()) (*it)->stop();
	}
	pBundle->stop();
}


} // namespace Poco::OSP::Shell
