//
// BundleActionCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  BundleActionCommand
//
// Definition of the BundleActionCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_BundleActionCommand_INCLUDED
#define OSP_Shell_BundleActionCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"


namespace Poco {
namespace OSP {
namespace Shell {


class BundleActionCommand: public OSPCommand
	/// This abstract class provides most of the functions
	/// required for implementing bundle actions (start, stop, etc.).
{
public:
	BundleActionCommand(Poco::OSP::BundleContext::Ptr pContext, const std::string& command);
		/// Creates the BundleActionCommand, using the given
		/// bundle context and command name.
		
	~BundleActionCommand();
		/// Destroys the BundleActionCommand.

protected:
	// AbstractCommand
	virtual int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr);
	
	[[nodiscard]] virtual int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) = 0;
		/// Perform the requested action on the bundle.
		///
		/// Returns Command::STATUS_OK if the action was performed, or another
		/// status of Command if not. The bundles that follow on the command line
		/// are then left unchanged.
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_BundleActionCommand_INCLUDED
