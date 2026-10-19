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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_InstallCommand_INCLUDED
#define OSP_Shell_InstallCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco {
namespace OSP {
namespace Shell {


class InstallCommand: public OSPCommand
	/// This command downloads and installs
	/// a bundle from a given URI.
{
public:
	InstallCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the InstallCommand, using the given
		/// bundle context.
		
	~InstallCommand();
		/// Destroys the InstallCommand.

protected:
	// AbstractCommand
	virtual int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr);
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_InstallCommand_INCLUDED
