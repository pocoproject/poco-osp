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
#include <map>


namespace Poco {
namespace OSP {
namespace Shell {


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
	///
	/// The following attributes can be specified for the extension point:
	///    * command: the name of the command (e.g., "bundles").
	///    * aliases: a comma-separated list of alias names for the command (e.g., "ls").
	///    * permission: the permission required to run this command.
	///    * class: the class name of the command factory (see the Command class).
	///    * library: the name of the shared library containing the command factory.
{
public:
	CommandExtensionPoint(CommandProcessor& processor);
		/// Creates the CommandExtensionPoint, using the given command processor and bundle context.

	~CommandExtensionPoint();
		/// Destroys the CommandExtensionPoint.

	// ExtensionPoint
	void handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem);

	static const std::string EXTPOINT;

protected:
	void handleCommand(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem);
		/// Handle the command extension point.

	void onBundleStopped(const void* pSender, Poco::OSP::BundleEvent& ev);
		/// When a bundle is stopped, all of its commands are automatically disabled.

	static const std::string ATTR_COMMAND;
	static const std::string ATTR_ALIASES;
	static const std::string ATTR_PERMISSION;
	static const std::string ATTR_CLASS;
	static const std::string ATTR_LIBRARY;
	static const std::string MANIFEST_NAME;

private:
	CommandExtensionPoint();

	using FactoryPtr = Poco::SharedPtr<CommandFactory>;
	using Loader = Poco::ClassLoader<CommandFactory>;
	using LibBundleMap = std::map<std::string, Poco::OSP::Bundle::Ptr>;

	CommandProcessor&  _commandProcessor;
	BundleContext::Ptr _pContext;
	Loader             _loader;
	LibBundleMap       _libBundleMap;
	Poco::FastMutex    _mutex;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_CommandExtensionPoint_INCLUDED
