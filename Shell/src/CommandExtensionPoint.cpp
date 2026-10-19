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
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Delegate.h"
#include <set>


using Poco::FastMutex;
using Poco::Delegate;
using Poco::StringTokenizer;


namespace Poco {
namespace OSP {
namespace Shell {


const std::string CommandExtensionPoint::EXTPOINT("com.appinf.osp.shell.command");
const std::string CommandExtensionPoint::ATTR_COMMAND("command");
const std::string CommandExtensionPoint::ATTR_ALIASES("aliases");
const std::string CommandExtensionPoint::ATTR_PERMISSION("permission");
const std::string CommandExtensionPoint::ATTR_CLASS("class");
const std::string CommandExtensionPoint::ATTR_LIBRARY("library");
const std::string CommandExtensionPoint::MANIFEST_NAME("Shell");


CommandExtensionPoint::CommandExtensionPoint(CommandProcessor& commandProcessor):
	_commandProcessor(commandProcessor),
	_pContext(commandProcessor.context())
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
	FastMutex::ScopedLock lock(_mutex);

	if (pExtensionElem)
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
	if (!_loader.isLibraryLoaded(libraryPath))
	{
		_loader.loadLibrary(libraryPath, MANIFEST_NAME);
		_libBundleMap[libraryPath] = pBundle;
	}
	
	FactoryPtr pFactory = _loader.create(className);
	Poco::OSP::BundleContext::Ptr pContext = _pContext->contextForBundle(pBundle);
	pFactory->init(pContext);
	
	_commandProcessor.addCommand(command, aliasesSet, pFactory, permission);
}


void CommandExtensionPoint::onBundleStopped(const void* pSender, Poco::OSP::BundleEvent& ev)
{
	FastMutex::ScopedLock lock(_mutex);
	
	Bundle::ConstPtr pBundle = ev.bundle();
	LibBundleMap::iterator it = _libBundleMap.begin();
	while (it != _libBundleMap.end())
	{
		if (it->second == pBundle)
		{
			_commandProcessor.removeBundle(it->second);
			_loader.unloadLibrary(it->first);
			LibBundleMap::iterator delIt(it);
			++it;
			_libBundleMap.erase(delIt);
		}
		else ++it;
	}
}


} } } // namespace Poco::OSP::Shell
