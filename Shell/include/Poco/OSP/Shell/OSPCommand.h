//
// OSPCommand.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  OSPCommand
//
// Definition of the OSPCommand class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Shell_OSPCommand_INCLUDED
#define OSP_Shell_OSPCommand_INCLUDED


#include "Poco/OSP/Shell/AbstractCommand.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include <vector>


namespace Poco::OSP::Shell {


class OSPShell_API OSPCommand: public AbstractCommand
	/// OSPCommand is the base class for all
	/// OSP-specific commands.
	///
	/// It provides various helper functions that
	/// make implementing commands that deal with OSP
	/// internals easier.
{
public:
	OSPCommand(Poco::OSP::BundleContext::Ptr pContext, const std::string& name);
		/// Creates the OSPCommand, using the given
		/// bundle context and name.
		///
		/// The name is used to load the command description
		/// and usage string from the bundle properties, using
		/// the <name>.description and <name>.usage properties.
		
	~OSPCommand() override;
		/// Destroys the OSPCommand.

	[[nodiscard]] Poco::OSP::BundleContext::Ptr context() const;
		/// Returns the bundle context.
		
protected:
	[[nodiscard]] Poco::OSP::Bundle::Ptr findBundle(const std::string& symbolicNameOrId) const;
		/// Finds a bundle, either by symbolic name or ID.
		///
		/// If the given argument is an integer, it is treated as bundle ID.
		/// Otherwise, it is treated as symbolic name.
		///
		/// Returns a pointer to the bundle that matches the symbolic name or
		/// id, or a null pointer otherwise.
		
	[[nodiscard]] bool isRequiredByShell(const Poco::OSP::Bundle::Ptr& pBundle) const;
		/// Returns true if the given bundle is the bundle of this command or a
		/// bundle that it requires, directly or indirectly. The bundle of a
		/// shell command requires the shell bundle, so this includes the shell
		/// bundle and the bundles that the shell bundle requires.
		///
		/// A command must not stop, uninstall or replace such a bundle from a
		/// shell session: that would remove code that runs the command or the
		/// session.

	[[nodiscard]] static bool match(const std::string& pattern, const std::string& subject);
		/// Returns true if the given subject matches the given pattern.
		///
		/// Every subject matches an empty pattern.

	[[nodiscard]] static bool match(const std::vector<std::string>& patterns, const std::string& subject);
		/// Returns true if the given subject matches at least one of the given pattern.
		///
		/// Every subject matches an empty pattern.
		
private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


//
// inlines
//
inline Poco::OSP::BundleContext::Ptr OSPCommand::context() const
{
	return _pContext;
}


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_OSPCommand_INCLUDED
