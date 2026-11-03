//
// InstallCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  InstallCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "InstallCommand.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/BundleInstallerService.h"
#include <ostream>


using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::BundleInstallerService;


namespace Poco::OSP::Shell {


InstallCommand::InstallCommand(Poco::OSP::BundleContext::Ptr pContext):
	OSPCommand(pContext, "install")
{
}


InstallCommand::~InstallCommand() = default;


int InstallCommand::executeImpl(Session& /*session*/, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.empty())
	{
		ostr << "At least one URI must be specified." << '\n';
		return Command::STATUS_USAGE;
	}
	
	ServiceRef::Ptr pInstallerRef = context()->registry().findByName(BundleInstallerService::SERVICE_NAME);
	if (pInstallerRef != nullptr)
	{
		Poco::AutoPtr<BundleInstallerService> pInstaller = pInstallerRef->castedInstance<BundleInstallerService>();
	
		for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it)
		{
			try
			{
				ostr << "Downloading and installing bundle from " << *it << '\n' << std::flush;
				Poco::OSP::Bundle::Ptr pBundle = pInstaller->installBundle(*it);
				ostr << "Bundle successfully installed: " << pBundle->symbolicName() << "/" << pBundle->version().toString() 
				     << " (ID: " << pBundle->id() << ")" << '\n';
			}
			catch (Poco::Exception& exc)
			{
				ostr << exc.displayText() << '\n';
				return Command::STATUS_ERROR;
			}
		}
		return Command::STATUS_OK;
	}
	else
	{
		ostr << "BundleInstallerService not found." << '\n';
		return Command::STATUS_ERROR;
	}
}


} // namespace Poco::OSP::Shell
