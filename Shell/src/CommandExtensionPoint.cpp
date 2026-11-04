//
// CommandExtensionPoint.cpp
//
// Library: OSP/Shell
// Package: OSPShell
// Module:  CommandExtensionPoint
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "CommandExtensionPoint.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Bugcheck.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>


using Poco::FastMutex;
using Poco::Delegate;
using Poco::StringTokenizer;


namespace Poco::OSP::Shell {


namespace
{
	using LibraryPtr = std::shared_ptr<const void>;
		/// An object that keeps a library of commands loaded while it exists.

	class LibraryCommand: public Command
		/// Runs a command created from a library and keeps the library
		/// loaded until the command is destroyed.
	{
	public:
		LibraryCommand() = delete;

		LibraryCommand(LibraryPtr pLibrary, std::unique_ptr<Command> pCommand):
			_pLibrary(std::move(pLibrary)),
			_pCommand(std::move(pCommand))
		{
		}

		~LibraryCommand() override = default;

		LibraryCommand(const LibraryCommand&) = delete;
		LibraryCommand(LibraryCommand&&) = delete;
		LibraryCommand& operator = (const LibraryCommand&) = delete;
		LibraryCommand& operator = (LibraryCommand&&) = delete;

		[[nodiscard]] int execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override
		{
			return _pCommand->execute(session, args, ostr);
		}

	private:
		// Declared first, so that the library is released after the command is destroyed.
		LibraryPtr _pLibrary;
		std::unique_ptr<Command> _pCommand;
	};

	class LibraryCommandFactory: public CommandFactory
		/// Creates the commands of a factory created from a library and keeps
		/// the library loaded while the factory or one of its commands exists.
	{
	public:
		LibraryCommandFactory() = delete;

		LibraryCommandFactory(LibraryPtr pLibrary, std::unique_ptr<CommandFactory> pFactory):
			_pLibrary(std::move(pLibrary)),
			_pFactory(std::move(pFactory))
		{
		}

		~LibraryCommandFactory() override = default;

		LibraryCommandFactory(const LibraryCommandFactory&) = delete;
		LibraryCommandFactory(LibraryCommandFactory&&) = delete;
		LibraryCommandFactory& operator = (const LibraryCommandFactory&) = delete;
		LibraryCommandFactory& operator = (LibraryCommandFactory&&) = delete;

		[[nodiscard]] Command* createCommand() override
		{
			std::unique_ptr<Command> pCommand(_pFactory->createCommand());
			if (pCommand == nullptr) return nullptr;

			// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): the caller owns the command
			return new LibraryCommand(_pLibrary, std::move(pCommand));
		}

	private:
		// Declared first, so that the library is released after the factory is destroyed.
		LibraryPtr _pLibrary;
		std::unique_ptr<CommandFactory> _pFactory;
	};
}


const std::string CommandExtensionPoint::EXTPOINT("com.appinf.osp.shell.command");
const std::string CommandExtensionPoint::ATTR_COMMAND("command");
const std::string CommandExtensionPoint::ATTR_ALIASES("aliases");
const std::string CommandExtensionPoint::ATTR_PERMISSION("permission");
const std::string CommandExtensionPoint::ATTR_CLASS("class");
const std::string CommandExtensionPoint::ATTR_LIBRARY("library");
const std::string CommandExtensionPoint::MANIFEST_NAME("Shell");


CommandExtensionPoint::CommandExtensionPoint(CommandProcessor& commandProcessor):
	_commandProcessor(commandProcessor),
	_pContext(commandProcessor.context()),
	_pLoader(std::make_shared<Loader>())
{
	_pContext->events().bundleStopped += Delegate<CommandExtensionPoint, BundleEvent>(this, &CommandExtensionPoint::onBundleStopped);
}


CommandExtensionPoint::~CommandExtensionPoint()
{
	try
	{
		_pContext->events().bundleStopped -= Delegate<CommandExtensionPoint, BundleEvent>(this, &CommandExtensionPoint::onBundleStopped);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void CommandExtensionPoint::handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	if (pExtensionElem != nullptr)
	{
		const Poco::XML::XMLString& extPoint = pExtensionElem->getAttribute(ExtensionPointService::POINT_ATTR);
		if (extPoint == EXTPOINT)
			handleCommand(pBundle, pExtensionElem);
		else
			throw Poco::OSP::BundleException("Failed to parse extension point");
	}
}


void CommandExtensionPoint::handleCommand(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	const std::string& command     = pExtensionElem->getAttribute(ATTR_COMMAND);
	const std::string& aliases     = pExtensionElem->getAttribute(ATTR_ALIASES);
	const std::string& permission  = pExtensionElem->getAttribute(ATTR_PERMISSION);
	const std::string& className   = pExtensionElem->getAttribute(ATTR_CLASS);
	std::string library = pExtensionElem->getAttribute(ATTR_LIBRARY);
	
	if (library.empty())
		library = pBundle->symbolicName();

	std::set<std::string> aliasesSet;
	StringTokenizer tokenizer(aliases, ",;", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
	for (StringTokenizer::Iterator it = tokenizer.begin(); it != tokenizer.end(); ++it)
	{
		aliasesSet.insert(*it);
	}
			
	std::string libraryPath = _pContext->pathForLibrary(library);
	_pLoader->loadLibrary(libraryPath, MANIFEST_NAME);
	const LibraryPtr pLibrary(nullptr, [pLoader = _pLoader, libraryPath](const void*)
	{
		try
		{
			pLoader->unloadLibrary(libraryPath);
		}
		catch (...)
		{
			poco_unexpected();
		}
	});
	
	// Not _pLoader->create(): it would also find a class of the same name in the library of another bundle.
	const Loader::Manif& manifest = _pLoader->manifestFor(libraryPath);
	const auto itClass = manifest.find(className);
	if (itClass == manifest.end())
		throw Poco::NotFoundException("class " + className + " in library " + libraryPath);
	std::unique_ptr<CommandFactory> pLibraryFactory(itClass->create());
	Poco::OSP::BundleContext::Ptr pContext = _pContext->contextForBundle(pBundle);
	pLibraryFactory->init(pContext);
	FactoryPtr pFactory = new LibraryCommandFactory(pLibrary, std::move(pLibraryFactory));
	pFactory->init(pContext);

	bool bundleStopped = false;
	bool commandExists = false;
	{
		FastMutex::ScopedLock lock(_mutex);

		// The bundle may have stopped while the factory was created; onBundleStopped()
		// has then removed its commands already, and this one must not be added.
		bundleStopped = !pBundle->isActive();
		if (!bundleStopped)
		{
			try
			{
				_commandProcessor.addCommand(command, aliasesSet, pFactory, permission);
			}
			catch (Poco::ExistsException&)
			{
				commandExists = true;
			}
		}
	}
	if (bundleStopped)
	{
		if (_pContext->logger().debug())
		{
			std::string msg("The shell command ");
			msg += command;
			msg += " of bundle ";
			msg += pBundle->symbolicName();
			msg += " is not added: the bundle is no longer active.";
			_pContext->logger().debug(msg);
		}
	}
	else if (commandExists)
	{
		std::string msg("The shell command ");
		msg += command;
		msg += " of bundle ";
		msg += pBundle->symbolicName();
		msg += " is not added: a command with this name exists already.";
		_pContext->logger().error(msg);
	}
}


void CommandExtensionPoint::onBundleStopped(const void* /*pSender*/, Poco::OSP::BundleEvent& ev)
{
	FastMutex::ScopedLock lock(_mutex);

	_commandProcessor.removeBundle(ev.bundle());
}


} // namespace Poco::OSP::Shell
