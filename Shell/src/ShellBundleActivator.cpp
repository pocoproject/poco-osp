//
// ShellBundleActivator.cpp
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/LoginFailures.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/ShellServer.h"
#include "Poco/OSP/Shell/LoginCommandFactory.h"
#include "Poco/OSP/Shell/QuitCommandFactory.h"
#include "Poco/OSP/Shell/HelpCommandFactory.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/AutoPtr.h"
#include "Poco/SharedPtr.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Exception.h"
#include "Poco/SharedLibrary.h"
#include "Poco/Types.h"
#include "CommandExtensionPoint.h"
#include <chrono>
#include <memory>
#include <string>
#if defined(OSP_SHELL_ENABLE_SSH)
#include "Poco/SSH/SSHServer.h"
#include "Poco/SSH/SSHHostKeyManager.h"
#include "Poco/SSH/SSHTypes.h"
#include "Poco/OSP/Shell/SessionTracker.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Thread.h"
#include "SSHShellConnection.h"
#endif


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::OSP::PreferencesService;
using Poco::OSP::Properties;
using Poco::OSP::ExtensionPointService;
using Poco::AutoPtr;
using Poco::SharedPtr;


namespace Poco {
namespace OSP {
namespace Shell {


class ShellBundleActivator: public BundleActivator
	/// The BundleActivator for the Shell Bundle.
	///
	/// Installs the CommandExtensionPoint.
{
public:
	ShellBundleActivator()
	{
	}
	
	~ShellBundleActivator()
	{
	}
	
	void start(BundleContext::Ptr pContext)
	{
		ServiceRef::Ptr pPrefsSvcRef = pContext->registry().findByName("osp.core.preferences");
		std::string authServiceName  = pContext->thisBundle()->properties().getString("authServiceName");
		std::string interfc          = pContext->thisBundle()->properties().getString("interface");
		Poco::UInt16 port            = pContext->thisBundle()->properties().getInt("port");
		int maxConnections           = pContext->thisBundle()->properties().getInt("maxConnections", 4);
		int sessionTimeout           = pContext->thisBundle()->properties().getInt("sessionTimeout",
			static_cast<int>(DEFAULT_SESSION_TIMEOUT.count()));
		int stopTimeout              = pContext->thisBundle()->properties().getInt("stopTimeout",
			static_cast<int>(DEFAULT_STOP_TIMEOUT.count()));
		if (pPrefsSvcRef)
		{
			Poco::AutoPtr<PreferencesService> pPrefsSvc = pPrefsSvcRef->castedInstance<PreferencesService>();
			authServiceName = pPrefsSvc->configuration()->getString("com.appinf.osp.shell.authServiceName", authServiceName);
			interfc         = pPrefsSvc->configuration()->getString("com.appinf.osp.shell.interface", interfc);
			port            = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.port", port);
			maxConnections  = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.maxConnections", maxConnections);
			sessionTimeout  = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.sessionTimeout",
				sessionTimeout);
			stopTimeout     = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.stopTimeout", stopTimeout);
		}
		_stopTimeout = std::chrono::seconds(stopTimeout);

		_pCommandProcessor = std::make_unique<CommandProcessor>(pContext);
		_pLoginFailures = std::make_unique<LoginFailures>();
		SharedPtr<CommandFactory> pHelpCommandFactory = new HelpCommandFactory(*_pCommandProcessor);
		pHelpCommandFactory->init(pContext);
		std::set<std::string> helpAliases;
		helpAliases.insert("h");
		helpAliases.insert("?");
		_pCommandProcessor->addCommand("help", helpAliases, pHelpCommandFactory, "");

		ServiceRef::Ptr pXPSRef = pContext->registry().findByName("osp.core.xp");
		if (pXPSRef)
		{
			AutoPtr<ExtensionPointService> pXPS = pXPSRef->castedInstance<ExtensionPointService>();
			_pCommandExtensionPoint = new CommandExtensionPoint(*_pCommandProcessor);
			pXPS->registerExtensionPoint(pContext->thisBundle(), CommandExtensionPoint::EXTPOINT, _pCommandExtensionPoint);

#if defined(OSP_SHELL_ENABLE_SSH)
			bool sshEnabled = false;
			if (pPrefsSvcRef)
			{
				Poco::AutoPtr<PreferencesService> pPrefsSvc = pPrefsSvcRef->castedInstance<PreferencesService>();
				sshEnabled = pPrefsSvc->configuration()->getBool("openSSH.server.enabled", false);
			}
			if (sshEnabled)
			{
				Poco::AutoPtr<PreferencesService> pPrefsSvc = pPrefsSvcRef->castedInstance<PreferencesService>();
				Poco::SSH::SSHServerConfig sshConfig;
				sshConfig.bindAddress       = pPrefsSvc->configuration()->getString("openSSH.server.interface", interfc);
				sshConfig.port              = pPrefsSvc->configuration()->getInt("openSSH.server.port", port);
				sshConfig.maxConnections    = pPrefsSvc->configuration()->getInt("openSSH.server.maxConnections", maxConnections);
				sshConfig.maxAuthAttempts   = pPrefsSvc->configuration()->getInt("openSSH.server.maxAuthAttempts", 3);
				sshConfig.enablePasswordAuth = pPrefsSvc->configuration()->getBool("openSSH.server.passwordAuth", true);
				sshConfig.enablePubkeyAuth  = pPrefsSvc->configuration()->getBool("openSSH.server.pubkeyAuth", true);
				sshConfig.authorizedKeysDir = pPrefsSvc->configuration()->getString("openSSH.server.authorizedKeysDir", "");
				const std::chrono::seconds loginGraceTime(pPrefsSvc->configuration()->getInt(
					"openSSH.server.loginGraceTime", static_cast<int>(DEFAULT_LOGIN_GRACE_TIME.count())));
				std::string hostKeyDir      = pPrefsSvc->configuration()->getString("openSSH.server.hostKeyDir",
				                               pContext->persistentDirectory().toString());
				sshConfig.hostKeyPath = Poco::SSH::SSHHostKeyManager::ensureHostKey(hostKeyDir);

				// SSHShellConnection checks the passwords with the authentication service.
				const ShellConnection::Config config{
					*_pCommandProcessor, authServiceName, *_pLoginFailures, std::chrono::seconds(sessionTimeout)};
				_pSSHSessions = std::make_unique<SessionTracker>();
				SessionTracker& sessions = *_pSSHSessions;
				_pSSHServer.reset(new Poco::SSH::SSHServer(sshConfig,
					[config, &sessions, loginGraceTime](
						ssh_session s, const Poco::SSH::SSHServerConfig& cfg, Poco::SSH::SSHServer& srv)
						-> Poco::SSH::SSHSession*
					{
						return new SSHShellConnection(s, cfg, srv, config, sessions, loginGraceTime);
					}
				));
				_pSSHServer->start();
				pContext->logger().information("SSH shell server listening on " +
					sshConfig.bindAddress + ":" + std::to_string(sshConfig.port));
			}
			else
#endif
			{
				Poco::Net::SocketAddress addr(interfc, port);
				Poco::Net::ServerSocket serverSocket(addr);
				const ShellConnection::Config config{
					*_pCommandProcessor, authServiceName, *_pLoginFailures, std::chrono::seconds(sessionTimeout)};
				_pShellServer = std::make_unique<ShellServer>(serverSocket, maxConnections, config);
				_pShellServer->start();
			}
		}
		else
		{
			pContext->logger().error("The ExtensionPointService is not available.");
		}		
	}
		
	void stop(BundleContext::Ptr pContext)
	{
		bool sessionsEnded = true;
#if defined(OSP_SHELL_ENABLE_SSH)
		if (_pSSHServer)
		{
			auto pStopper = std::make_unique<Poco::Thread>();
			if (SSHShellConnection::stopServer(*_pSSHServer, *_pSSHSessions, *pStopper, _stopTimeout))
			{
				_pSSHServer.reset();
			}
			else
			{
				std::string msg("SSH shell sessions have not ended within ");
				msg += Poco::NumberFormatter::format(
					static_cast<Poco::Int64>(std::chrono::milliseconds(_stopTimeout).count()));
				msg += " ms after the shell stopped; the SSH server stops when they end,";
				msg += " and the objects they use are kept.";
				pContext->logger().warning(msg);
				[[maybe_unused]] const Poco::Thread* pKeptStopper = pStopper.release();
				[[maybe_unused]] const Poco::SSH::SSHServer* pKeptSSHServer = _pSSHServer.release();
				[[maybe_unused]] const SessionTracker* pKeptSSHSessions = _pSSHSessions.release();
				sessionsEnded = false;
			}
		}
#endif
		if (_pShellServer != nullptr)
		{
			if (_pShellServer->stop(_stopTimeout) > 0)
			{
				[[maybe_unused]] const ShellServer* pKeptServer = _pShellServer.release();
				sessionsEnded = false;
			}
			_pShellServer.reset();
		}
		if (!sessionsEnded)
		{
			// The sessions that have not ended still use these objects, so they
			// are kept for the rest of the process; a command that runs keeps
			// the library it comes from loaded.
			[[maybe_unused]] const CommandProcessor* pKeptProcessor = _pCommandProcessor.release();
			[[maybe_unused]] const LoginFailures* pKeptLoginFailures = _pLoginFailures.release();
#if !defined(POCO_OSP_STATIC)
			// The kept objects run code of this library, which the bundle loader
			// unloads when it unloads the bundle. A SharedLibrary leaves its
			// library loaded when it is destroyed, so loading the library once
			// more keeps it loaded until the process ends.
			const Bundle::ConstPtr pBundle = pContext->thisBundle();
			const std::string& libraryName =
				pBundle->activatorLibrary().empty() ? pBundle->symbolicName() : pBundle->activatorLibrary();
			const std::string libraryPath = pContext->pathForLibrary(libraryName);
			try
			{
				const Poco::SharedLibrary library(libraryPath);
				std::string msg("The library ");
				msg += libraryPath;
				msg += " of the shell bundle stays loaded until the process ends,";
				msg += " because the shell has not stopped completely.";
				pContext->logger().information(msg);
			}
			catch (Poco::Exception& exc)
			{
				std::string msg("The library ");
				msg += libraryPath;
				msg += " of the shell bundle cannot be kept loaded for the sessions that have not ended: ";
				msg += exc.displayText();
				msg += ". The process may fail when the bundle loader unloads the bundle.";
				pContext->logger().error(msg);
			}
#endif
		}
	}
	
private:
	static constexpr std::chrono::seconds DEFAULT_SESSION_TIMEOUT{600};
	static constexpr std::chrono::seconds DEFAULT_STOP_TIMEOUT{10};

	AutoPtr<CommandExtensionPoint>    _pCommandExtensionPoint;
	std::unique_ptr<CommandProcessor> _pCommandProcessor;
	std::unique_ptr<LoginFailures>    _pLoginFailures;
	std::unique_ptr<ShellServer>      _pShellServer;
	std::chrono::seconds              _stopTimeout{DEFAULT_STOP_TIMEOUT};
#if defined(OSP_SHELL_ENABLE_SSH)
	static constexpr std::chrono::seconds DEFAULT_LOGIN_GRACE_TIME{60};

	std::unique_ptr<SessionTracker> _pSSHSessions;
	std::unique_ptr<Poco::SSH::SSHServer> _pSSHServer;
#endif
};


} } } // namespace Poco::OSP::Shell


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::LoginCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::QuitCommandFactory)
POCO_END_MANIFEST


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::ShellBundleActivator)
POCO_END_MANIFEST
