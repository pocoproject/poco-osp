//
// InstallCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  InstallCommand
//
// Definition of the InstallCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_InstallCommand_INCLUDED
#define OSP_Shell_InstallCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class InstallCommand: public OSPCommand
	/// This command downloads and installs
	/// a bundle from a given URI.
{
public:
	InstallCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the InstallCommand, using the given
		/// bundle context.
		
	~InstallCommand() override;
		/// Destroys the InstallCommand.

protected:
	// AbstractCommand
	[[nodiscard]] int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_InstallCommand_INCLUDED
