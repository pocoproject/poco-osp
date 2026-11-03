//
// ResolveCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ResolveCommand
//
// Definition of the ResolveCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_ResolveCommand_INCLUDED
#define OSP_Shell_ResolveCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class ResolveCommand: public BundleActionCommand
	/// This command resolves a bundle.
{
public:
	ResolveCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the ResolveCommand, using the given
		/// bundle context.
		
	~ResolveCommand() override;
		/// Destroys the ResolveCommand.

protected:
	// BundleActionCommand
	[[nodiscard]] int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_ResolveCommand_INCLUDED
