//
// InfoCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  InfoCommand
//
// Definition of the InfoCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_InfoCommand_INCLUDED
#define OSP_Shell_InfoCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class InfoCommand: public BundleActionCommand
	/// This command displays information about a bundle.
	///
	/// If the --dependencies option is given, the
	/// dependencies of the bundle are displayed.
	///
	/// If the --clients option is given, all
	/// bundles that depend on this bundle are shown.
{
public:
	InfoCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the InfoCommand, using the given
		/// bundle context.
		
	~InfoCommand() override;
		/// Destroys the InfoCommand.

protected:
	// BundleActionCommand
	[[nodiscard]] int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
	void defineOptions(Poco::Util::OptionSet& options) override;

	void handleDependencies(const std::string& name, const std::string& value);
	void handleClients(const std::string& name, const std::string& value);
	void doInfo(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr);
	void doDependencies(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr);
	void doClients(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr);
	
private:
	bool _dependencies = false;
	bool _clients = false;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_InfoCommand_INCLUDED
