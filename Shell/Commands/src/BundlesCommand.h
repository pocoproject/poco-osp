//
// BundlesCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  BundlesCommand
//
// Definition of the BundlesCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_BundlesCommand_INCLUDED
#define OSP_Shell_BundlesCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class BundlesCommand: public OSPCommand
	/// This command outputs a list of known bundles,
	/// optionally filtered by a wildcard pattern expression.
{
public:
	BundlesCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the BundlesCommand, using the given
		/// bundle context.
		
	~BundlesCommand() override;
		/// Destroys the BundlesCommand.

protected:
	// AbstractCommand
	[[nodiscard]] int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_BundlesCommand_INCLUDED
