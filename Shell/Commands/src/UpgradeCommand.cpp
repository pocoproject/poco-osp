//
// UpgradeCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  UpgradeCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "UpgradeCommand.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/BundleInstallerService.h"


using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::BundleInstallerService;


namespace Poco {
namespace OSP {
namespace Shell {


UpgradeCommand::UpgradeCommand(Poco::OSP::BundleContext::Ptr pContext):
	OSPCommand(pContext, "upgrade"),
	_force(false)
{
}


UpgradeCommand::~UpgradeCommand()
{
}


int UpgradeCommand::executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.size() == 2)
	{
		const std::string& symbolicNameOrId(args[0]);
		const std::string& uri(args[1]);
		
		ServiceRef::Ptr pInstallerRef = context()->registry().findByName(BundleInstallerService::SERVICE_NAME);
		if (pInstallerRef)
		{
			Poco::AutoPtr<BundleInstallerService> pInstaller = pInstallerRef->castedInstance<BundleInstallerService>();
	
			Poco::OSP::Bundle::Ptr pBundle = findBundle(symbolicNameOrId);
			if (pBundle)
			{
				if (!pBundle->isActive() || _force)
				{
					ostr << "Downloading and upgrading bundle " << pBundle->symbolicName() << " from " << uri << std::endl;
					pBundle = pInstaller->replaceBundle(pBundle->symbolicName(), uri);
					ostr << "Bundle successfully upgraded: " 
					     << pBundle->symbolicName() << "/" << pBundle->version().toString() 
					     << " (New ID: " << pBundle->id() << ")" << std::endl;
					return Command::STATUS_OK;
				}
				else
				{
					ostr << "Cannot upgrade an active bundle. Stop bundle or use --force option to override." << std::endl;
					return Command::STATUS_ERROR;
				}
			}
			else
			{
				ostr << "No such bundle: " << symbolicNameOrId << std::endl;
				return Command::STATUS_ERROR;
			}
		}
		else
		{
			ostr << "BundleInstallerService not found." << std::endl;
			return Command::STATUS_ERROR;
		}
	}
	else
	{
		ostr << "Bundle symbolic name or ID and an URI must be given." << std::endl;
		return Command::STATUS_USAGE;
	}
}


void UpgradeCommand::defineOptions(Poco::Util::OptionSet& options)
{
	OSPCommand::defineOptions(options);
	
	options.addOption(
		Poco::Util::Option("force", "f", "force upgrade of an active bundle")
			.required(false)
			.repeatable(false)
			.callback(Poco::Util::OptionCallback<UpgradeCommand>(this, &UpgradeCommand::handleForce)));
}


void UpgradeCommand::handleForce(const std::string& name, const std::string& value)
{
	_force = true;
}


} } } // namespace Poco::OSP::Shell
