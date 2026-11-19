//
// ShellFixture.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellFixture.h"
#include "TestCommandFactory.h"
#include "CommandExtensionPoint.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/HelpCommandFactory.h"
#include "Poco/OSP/Shell/LoginCommandFactory.h"
#include "Poco/OSP/Shell/QuitCommandFactory.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/ShellServer.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Properties.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/DOM/Element.h"
#include "Poco/DOM/Node.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/SAX/XMLReader.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/AutoPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>
#include <ios>
#include <istream>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using Poco::OSP::Shell::CommandProcessor;
using Poco::OSP::Shell::CommandExtensionPoint;
using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleFactory;
using Poco::OSP::BundleLoader;
using Poco::OSP::CodeCache;
using Poco::OSP::LanguageTag;
using Poco::Path;
using namespace std::string_literals;


namespace
{
	std::string manifest(const std::string& symbolicName, const std::string& requiredBundle,
		const std::string& activator, const std::string& headers)
	{
		std::string text =
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Shell Test Bundle\n"
			"Bundle-SymbolicName: " + symbolicName + "\n"
			"Bundle-Version: 1.0.0\n"
			"Bundle-Vendor: Applied Informatics\n"
			"Bundle-Copyright: (c) 2026, Applied Informatics Software Engineering GmbH\n";
		if (!requiredBundle.empty())
		{
			text += "Require-Bundle: " + requiredBundle + "\n";
		}
		if (!activator.empty())
		{
			text += "Bundle-Activator: " + activator + "\n";
		}
		return text + headers;
	}

	std::string sharedCodeCachePath()
		/// Returns the code cache that all fixtures of the process share. A library
		/// that the shell keeps loaded after a test stays loaded for the rest of the
		/// process; loading it again from another path would load a second copy,
		/// with a second definition of each of its classes.
	{
		static const Poco::TemporaryFile directory;
		return Path(directory.path()).makeDirectory().toString();
	}
}


const std::string ShellFixture::BASE_BUNDLE("osp.shell.test.base");
const std::string ShellFixture::SHELL_BUNDLE("com.appinf.osp.shell");
const std::string ShellFixture::COMMANDS_BUNDLE("com.appinf.osp.shell.commands");
const std::string ShellFixture::LOGLEVEL_BUNDLE("com.appinf.osp.shell.loglevelcommand");
const std::string ShellFixture::OTHER_BUNDLE("osp.shell.test.other");


ShellFixture::ShellFixture(const std::string& authServiceName):
	_authServiceName(authServiceName)
{
	Path directory(_directory.path());
	directory.makeDirectory();
	_pCodeCache = std::make_unique<CodeCache>(sharedCodeCachePath());
	_pBundleContextFactory = new BundleContextFactory(_registry, _systemEvents, Path(directory, "persistency/"s));
	_pLoader = std::make_unique<BundleLoader>(
		*_pCodeCache, new BundleFactory(LanguageTag("en"s, "US"s)), _pBundleContextFactory);

	createBundle(BASE_BUNDLE, ""s, ""s);
	createBundle(SHELL_BUNDLE, BASE_BUNDLE, "bundle"s);
	createBundle(COMMANDS_BUNDLE, SHELL_BUNDLE, "Commands/bundle"s);
	createBundle(LOGLEVEL_BUNDLE, SHELL_BUNDLE, "LogLevelCommand/bundle"s);
	createBundle(OTHER_BUNDLE, ""s, ""s);
	installLibrary(COMMANDS_BUNDLE, OSPSHELL_TEST_COMMANDS_LIBRARY);
	installLibrary(LOGLEVEL_BUNDLE, OSPSHELL_TEST_LOGLEVEL_LIBRARY);
	_pLoader->resolveAllBundles();
	_pLoader->startAllBundles();
	const Path repositoryPath(directory, "repository/"s);
	Poco::File(repositoryPath).createDirectories();
	_pRepository = std::make_unique<Poco::OSP::BundleRepository>(repositoryPath.toString(), *_pLoader);
	_registry.registerService(
		Poco::OSP::BundleInstallerService::SERVICE_NAME,
		new Poco::OSP::BundleInstallerService(*_pRepository),
		Poco::OSP::Properties());

	_pShellContext = _pLoader->contextForBundle(bundle(SHELL_BUNDLE));
	_pProcessor = std::make_unique<CommandProcessor>(_pShellContext);
	// The shell bundle adds help itself and login and quit through its extensions.
	CommandProcessor::FactoryPtr pHelpFactory = new Poco::OSP::Shell::HelpCommandFactory(*_pProcessor);
	pHelpFactory->init(_pShellContext);
	_pProcessor->addCommand("help"s, {"h"s, "?"s}, pHelpFactory, ""s);
	CommandProcessor::FactoryPtr pLoginFactory = new Poco::OSP::Shell::LoginCommandFactory;
	pLoginFactory->init(_pShellContext);
	_pProcessor->addCommand("login"s, {}, pLoginFactory, ""s);
	CommandProcessor::FactoryPtr pQuitFactory = new Poco::OSP::Shell::QuitCommandFactory;
	pQuitFactory->init(_pShellContext);
	_pProcessor->addCommand("quit"s, {"exit"s}, pQuitFactory, ""s);

	_pExtensionPoint = new CommandExtensionPoint(*_pProcessor);
	addExtensions(COMMANDS_BUNDLE);
	addExtensions(LOGLEVEL_BUNDLE);
}


ShellFixture::~ShellFixture()
{
	try
	{
		stopServer();
		_registry.unregisterService(Poco::OSP::BundleInstallerService::SERVICE_NAME);
		// The extension point service handles events of the loader, so it must go first.
		if (_registry.findByName(Poco::OSP::ExtensionPointService::SERVICE_NAME) != nullptr)
		{
			_registry.unregisterService(Poco::OSP::ExtensionPointService::SERVICE_NAME);
		}
	}
	catch (...)
	{
		poco_unexpected();
	}
}


Poco::OSP::Bundle::Ptr ShellFixture::bundle(const std::string& symbolicName) const
{
	Poco::OSP::Bundle::Ptr pBundle = _pLoader->findBundle(symbolicName);
	if (pBundle == nullptr) throw Poco::NotFoundException("bundle"s, symbolicName);
	return pBundle;
}


void ShellFixture::addCommand(const std::string& command, const std::string& permission,
	TestCommandFactory::Function function)
{
	CommandProcessor::FactoryPtr pFactory = new TestCommandFactory(std::move(function));
	pFactory->init(_pShellContext);
	_pProcessor->addCommand(command, {}, pFactory, permission);
}


void ShellFixture::addBundle(const std::string& symbolicName, const std::string& requiredBundle,
	const std::string& headers)
{
	createBundle(symbolicName, requiredBundle, ""s, ""s, ""s, headers);
	Poco::OSP::Bundle::Ptr pBundle = bundle(symbolicName);
	pBundle->resolve();
	pBundle->start();
}


void ShellFixture::addCommandBundle(const std::string& symbolicName, const std::string& sourceDirectory,
	const std::string& libraryPath)
{
	createBundle(symbolicName, SHELL_BUNDLE, sourceDirectory);
	installLibrary(symbolicName, libraryPath);
	Poco::OSP::Bundle::Ptr pBundle = bundle(symbolicName);
	pBundle->resolve();
	pBundle->start();
	addExtensions(symbolicName);
}


void ShellFixture::addExtensions(const std::string& symbolicName)
{
	Poco::OSP::Bundle::Ptr pBundle = bundle(symbolicName);
	std::unique_ptr<std::istream> pStream(pBundle->getResource("extensions.xml"s));
	if (pStream == nullptr) throw Poco::NotFoundException("extensions.xml"s, symbolicName);
	Poco::XML::DOMParser parser;
	parser.setFeature(Poco::XML::XMLReader::FEATURE_NAMESPACES, false);
	parser.setFeature(Poco::XML::DOMParser::FEATURE_FILTER_WHITESPACE, true);
	Poco::XML::InputSource source(*pStream);
	Poco::AutoPtr<Poco::XML::Document> pDocument(parser.parse(&source));
	for (Poco::XML::Node* pNode = pDocument->documentElement()->firstChild(); pNode != nullptr;
		pNode = pNode->nextSibling())
	{
		if (auto* pElement = dynamic_cast<Poco::XML::Element*>(pNode); pElement != nullptr)
		{
			_pExtensionPoint->handleExtension(pBundle, pElement);
		}
	}
}


Poco::OSP::Bundle::Ptr ShellFixture::addShellBundle(const std::string& symbolicName, const std::string& properties)
{
	_registry.registerService(
		Poco::OSP::ExtensionPointService::SERVICE_NAME,
		new Poco::OSP::ExtensionPointService(_pLoader->events()),
		Poco::OSP::Properties());
	createBundle(symbolicName, BASE_BUNDLE, "bundle"s, "Poco::OSP::Shell::ShellBundleActivator"s, properties);
	installLibrary(symbolicName, OSPSHELL_TEST_SHELL_LIBRARY);
	Poco::OSP::Bundle::Ptr pBundle = bundle(symbolicName);
	pBundle->resolve();
	pBundle->start();
	return pBundle;
}


Poco::UInt16 ShellFixture::freePort()
{
	const Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
	return socket.address().port();
}


Poco::OSP::Shell::ShellConnection::Config ShellFixture::connectionConfig(std::chrono::seconds sessionTimeout)
{
	return Poco::OSP::Shell::ShellConnection::Config{*_pProcessor, _authServiceName, _loginFailures, sessionTimeout};
}


Poco::UInt16 ShellFixture::startServer(std::chrono::seconds sessionTimeout)
{
	constexpr int MAX_CONNECTIONS = 4;
	const Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
	_pServer = std::make_unique<Poco::OSP::Shell::ShellServer>(
		serverSocket, MAX_CONNECTIONS, connectionConfig(sessionTimeout));
	_pServer->start();
	return _pServer->port();
}


std::size_t ShellFixture::stopServer(std::chrono::milliseconds timeout)
{
	if (_pServer == nullptr) return 0;

	const std::size_t activeSessions = _pServer->stop(timeout);
	if (activeSessions == 0) _pServer.reset();
	return activeSessions;
}


void ShellFixture::stopServer()
{
	constexpr std::chrono::seconds STOP_TIMEOUT{10};
	if (stopServer(STOP_TIMEOUT) > 0)
	{
		throw Poco::IllegalStateException("shell sessions have not ended"s);
	}
}


void ShellFixture::createBundle(const std::string& symbolicName, const std::string& requiredBundle,
	const std::string& sourceDirectory,
	const std::string& activator, const std::string& properties, const std::string& headers)
{
	Path bundlePath(_directory.path());
	bundlePath.makeDirectory();
	bundlePath.pushDirectory(symbolicName + "_1.0.0"s);
	Poco::File(bundlePath).createDirectories();
	if (!sourceDirectory.empty())
	{
		Path sourcePath(OSPSHELL_TEST_SHELL_DIR, Path::PATH_UNIX);
		sourcePath.makeDirectory();
		sourcePath.append(Path(sourceDirectory, Path::PATH_UNIX));
		sourcePath.makeDirectory();
		std::vector<std::string> files;
		Poco::File(sourcePath).list(files);
		for (const auto& file: files)
		{
			Poco::File(Path(sourcePath, file)).copyTo(bundlePath.toString());
		}
	}
	if (!properties.empty())
	{
		// Properties defined later in the file replace those defined before.
		Poco::FileOutputStream propertiesStream(Path(bundlePath, "bundle.properties"s).toString(), std::ios::app);
		propertiesStream << '\n' << properties;
		propertiesStream.close();
	}
	const Path manifestPath(bundlePath, Path("META-INF/manifest.mf"s, Path::PATH_UNIX));
	Poco::File(manifestPath.parent()).createDirectories();
	Poco::FileOutputStream ostr(manifestPath.toString());
	ostr << manifest(symbolicName, requiredBundle, activator, headers);
	ostr.close();
	_pLoader->loadBundle(bundlePath.toString());
}


void ShellFixture::installLibrary(const std::string& symbolicName, const std::string& path)
{
	const Poco::File library(_pLoader->pathForLibrary(symbolicName));
	// An earlier fixture may have installed the library, which may still be loaded.
	if (library.exists()) return;

	Poco::File(Path(library.path()).parent()).createDirectories();
	Poco::File(path).copyTo(library.path());
}
