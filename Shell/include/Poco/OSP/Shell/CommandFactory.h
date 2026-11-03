//
// CommandFactory.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  CommandFactory
//
// Definition of the CommandFactory class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_CommandFactory_INCLUDED
#define OSP_Shell_CommandFactory_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/BundleContext.h"


namespace Poco::OSP::Shell {


class Command;


class OSPShell_API CommandFactory
	/// A factory for Command objects.
{
public:
	CommandFactory();
		/// Creates the CommandFactory.
		
	virtual ~CommandFactory();
		/// Destroys the CommandFactory.

	CommandFactory(const CommandFactory&) = delete;
	CommandFactory& operator = (const CommandFactory&) = delete;
		
	[[nodiscard]] virtual Command* createCommand() = 0;
		/// Create a Command object.

	void init(Poco::OSP::BundleContext::Ptr pContext);
		/// Initializes the factory with the given BundleContext.

	[[nodiscard]] Poco::OSP::BundleContext::Ptr context() const;
		/// Returns the context. The pointer is guaranteed to be valid.

protected:	
	virtual void initImpl();
		/// Add extra initialization code by overriding this method in your subclass.

private:
	BundleContext::Ptr _pContext;
};


//
// inlines
//
inline Poco::OSP::BundleContext::Ptr CommandFactory::context() const
{
	return _pContext;
}


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_CommandFactory_INCLUDED
