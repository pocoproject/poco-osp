//
// StartCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  StartCommand
//
// Definition of the StartCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_StartCommand_INCLUDED
#define OSP_Shell_StartCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco {
namespace OSP {
namespace Shell {


class StartCommand: public BundleActionCommand
	/// This command starts a bundle.
{
public:
	StartCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the StartCommand, using the given
		/// bundle context.
		
	~StartCommand();
		/// Destroys the StartCommand.

protected:
	// BundleActionCommand
	int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_StartCommand_INCLUDED
