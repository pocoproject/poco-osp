//
// ServicesCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  ServicesCommand
//
// Definition of the ServicesCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_ServicesCommand_INCLUDED
#define OSP_Shell_ServicesCommand_INCLUDED


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"


namespace Poco {
namespace OSP {
namespace Shell {


class ServicesCommand: public OSPCommand
	/// This command outputs a list of known services,
	/// optionally filtered by a wildcard pattern expression.
	///
	/// The command supports a --query option that allows to
	/// specify a service registry query string for finding
	/// specific services.
{
public:
	ServicesCommand(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the ServicesCommand, using the given
		/// bundle context.
		
	~ServicesCommand();
		/// Destroys the ServicesCommand.

protected:
	// AbstractCommand
	virtual int executeImpl(Session& session, const std::vector<std::string>& args, std::ostream& ostr);
	virtual void defineOptions(Poco::Util::OptionSet& options);
	
	void handleQuery(const std::string& name, const std::string& value);
	void handleProperties(const std::string& name, const std::string& value);
	
private:
	std::string _query;
	bool _properties;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_ServicesCommand_INCLUDED
