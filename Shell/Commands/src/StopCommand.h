//
// StopCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  StopCommand
//
// Definition of the StopCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_StopCommand_INCLUDED
#define OSP_Shell_StopCommand_INCLUDED


#include "BundleActionCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco::OSP::Shell {


class StopCommand: public BundleActionCommand
	/// This command stops a bundle.
	///
	/// Before stopping the bundle, dependencies are
	/// checked. The bundle is not stopped if there are
	/// other started bundles that require this bundle,
	/// unless the --force option is specified.
{
public:
	StopCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the StopCommand, using the given
		/// bundle context.

	~StopCommand() override;
		/// Destroys the StopCommand.

protected:
	// BundleActionCommand
	[[nodiscard]] int doAction(Poco::OSP::Bundle::Ptr pBundle, std::ostream& ostr) override;
	void defineOptions(Poco::Util::OptionSet& options) override;

	using BundleVec = std::vector<Poco::OSP::Bundle::Ptr>;

	void dependingBundles(Poco::OSP::Bundle::Ptr pBundle, BundleVec& dependingBundles);
	void stopAll(Poco::OSP::Bundle::Ptr pBundle);

	void handleForce(const std::string& name, const std::string& value);
	void handleAll(const std::string& name, const std::string& value);

private:
	bool _force = false;
	bool _all = false;
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_StopCommand_INCLUDED
