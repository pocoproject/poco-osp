//
// CommandExtensionPoint.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  CommandExtensionPoint
//
// Definition of the CommandExtensionPoint class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_CommandExtensionPoint_INCLUDED
#define OSP_Shell_CommandExtensionPoint_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/ExtensionPoint.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/ClassLoader.h"
#include "Poco/Mutex.h"
#include "Poco/SharedPtr.h"
#include <memory>


namespace Poco::OSP::Shell {


class CommandExtensionPoint: public ExtensionPoint
	/// CommandExtensionPoint is used to add commands to the
	/// OSP Shell.
	///
	/// To register a command:
	///     <extension point="com.appinf.osp.shell.command"
	///                command="<command>"
	///                aliases="<list of aliases>"
	///                permission="<required permission>"
	///                class="<factory class>"
	///                library="<library name>"/>
	///
	/// Note that the extension points are automatically removed if a bundle is stopped.
	/// A command that runs keeps the library it comes from loaded until it ends.
	///
	/// The following attributes can be specified for the extension point:
	///    * command: the name of the command (e.g., "bundles").
	///    * aliases: a comma-separated list of alias names for the command (e.g., "ls").
	///    * permission: the permission required to run this command.
	///      Without a permission, the command runs in every session after the login.
	///    * class: the class name of the command factory (see the Command class).
	///    * library: the name of the shared library containing the command factory.
	///
	/// The command factory is created from the Shell manifest of the given library. handleExtension()
	/// throws a Poco::LibraryLoadException if the library cannot be opened or has no Shell manifest,
	/// and a Poco::NotFoundException if the manifest does not contain the class.
	///
	/// On Linux, unless the libraries are built with hidden visibility, the command factory classes
	/// and command classes of different bundles need different qualified names; otherwise a class
	/// may use the code of the library that was loaded first.
{
public:
	CommandExtensionPoint(CommandProcessor& processor);
		/// Creates the CommandExtensionPoint, using the given command processor and its bundle context.

	~CommandExtensionPoint() override;
		/// Destroys the CommandExtensionPoint.

	CommandExtensionPoint() = delete;

	// ExtensionPoint
	void handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem) override;

	static const std::string EXTPOINT;

protected:
	void handleCommand(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem);
		/// Handle the command extension point.
		///
		/// A command whose name another command has already is not added;
		/// this is logged as an error. A command of a bundle that is no longer
		/// active, for example because it has stopped while the command factory
		/// was created, is not added either.

	void onBundleStopped(const void* pSender, Poco::OSP::BundleEvent& ev);
		/// When a bundle is stopped, all of its commands are automatically disabled.

	static const std::string ATTR_COMMAND;
	static const std::string ATTR_ALIASES;
	static const std::string ATTR_PERMISSION;
	static const std::string ATTR_CLASS;
	static const std::string ATTR_LIBRARY;
	static const std::string MANIFEST_NAME;

private:
	using FactoryPtr = Poco::SharedPtr<CommandFactory>;
	using Loader = Poco::ClassLoader<CommandFactory>;

	CommandProcessor&       _commandProcessor;
	BundleContext::Ptr      _pContext;
	std::shared_ptr<Loader> _pLoader;
	Poco::FastMutex         _mutex;
		/// Held only to add a command to the command processor and while
		/// onBundleStopped() removes the commands of a bundle, never while a
		/// library is loaded, a factory is created or another bundle context
		/// is looked up: those take locks of the framework, which in turn may
		/// be held while the framework calls onBundleStopped().
};


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_CommandExtensionPoint_INCLUDED
