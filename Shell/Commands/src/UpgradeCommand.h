//
// UpgradeCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  UpgradeCommand
//
// Definition of the UpgradeCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_UpgradeCommand_INCLUDED
#define OSP_Shell_UpgradeCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/Bundle.h"


namespace Poco {
namespace OSP {
namespace Shell {


class UpgradeCommand: public OSPCommand
	/// This command downloads and upgrades
	/// an existing bundle from a given URI.
{
public:
	UpgradeCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the UpgradeCommand, using the given
		/// bundle context.
		
	~UpgradeCommand();
		/// Destroys the UpgradeCommand.

protected:
	// AbstractCommand
	virtual int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr);
	virtual void defineOptions(Poco::Util::OptionSet& options);

	void handleForce(const std::string& name, const std::string& value);
	
private:
	bool _force;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_UpgradeCommand_INCLUDED
