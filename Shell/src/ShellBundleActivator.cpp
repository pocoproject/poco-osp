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
#include "Poco/OSP/Shell/ShellConnectionFactory.h"
#include "Poco/OSP/Shell/LoginCommandFactory.h"
#include "Poco/OSP/Shell/QuitCommandFactory.h"
#include "Poco/OSP/Shell/HelpCommandFactory.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/AutoPtr.h"
#include "Poco/SharedPtr.h"
#include "Poco/ClassLibrary.h"
#include "CommandExtensionPoint.h"
#include <memory>
#if defined(OSP_SHELL_ENABLE_SSH)
#include "Poco/SSH/SSHServer.h"
#include "Poco/SSH/SSHHostKeyManager.h"
#include "Poco/OSP/Shell/Session.h"
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
		int maxConnections           = pContext->thisBundle()->properties().getInt("maxConnections", 1);
		if (pPrefsSvcRef)
		{
			Poco::AutoPtr<PreferencesService> pPrefsSvc = pPrefsSvcRef->castedInstance<PreferencesService>();
			authServiceName = pPrefsSvc->configuration()->getString("com.appinf.osp.shell.authServiceName", authServiceName);
			interfc         = pPrefsSvc->configuration()->getString("com.appinf.osp.shell.interface", interfc);
			port            = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.port", port);
			maxConnections  = pPrefsSvc->configuration()->getInt("com.appinf.osp.shell.maxConnections", maxConnections);
		}

		_pCommandProcessor = new CommandProcessor(pContext);
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
				std::string hostKeyDir      = pPrefsSvc->configuration()->getString("openSSH.server.hostKeyDir",
				                               pContext->persistentDirectory().toString());
				sshConfig.hostKeyPath = Poco::SSH::SSHHostKeyManager::ensureHostKey(hostKeyDir);

				// Set up password authenticator using OSP auth service
				std::string authSvcName = authServiceName;
				auto pCtx = pContext;
				sshConfig.passwordAuthenticator = [pCtx, authSvcName](const std::string& user, const std::string& pass) -> bool
				{
					Session session(pCtx, authSvcName);
					return session.login(user, pass);
				};

				CommandProcessor& cp = *_pCommandProcessor;
				_pSSHServer.reset(new Poco::SSH::SSHServer(sshConfig,
					[&cp, authSvcName](ssh_session s, const Poco::SSH::SSHServerConfig& cfg, Poco::SSH::SSHServer& srv) -> Poco::SSH::SSHSession*
					{
						return new SSHShellConnection(s, cfg, srv, cp, authSvcName);
					}
				));
				_pSSHServer->start();
				pContext->logger().information("SSH shell server listening on " +
					sshConfig.bindAddress + ":" + std::to_string(sshConfig.port));
			}
			else
#endif
			{
				Poco::Net::TCPServerParams::Ptr pParams = new Poco::Net::TCPServerParams;
				pParams->setMaxThreads(maxConnections);
				pParams->setMaxQueued(1);
				Poco::Net::SocketAddress addr(interfc, port);
				Poco::Net::ServerSocket serverSocket(addr);
				_pTCPServer = new Poco::Net::TCPServer(new ShellConnectionFactory(*_pCommandProcessor, authServiceName), serverSocket, pParams);
				_pTCPServer->start();
			}
		}
		else
		{
			pContext->logger().error("The ExtensionPointService is not available.");
		}		
	}
		
	void stop(BundleContext::Ptr pContext)
	{
#if defined(OSP_SHELL_ENABLE_SSH)
		if (_pSSHServer)
		{
			_pSSHServer->stop();
			_pSSHServer.reset();
		}
#endif
		if (_pTCPServer)
		{
			_pTCPServer->stop();
			_pTCPServer = 0;
		}
	}
	
private:
	AutoPtr<CommandExtensionPoint>  _pCommandExtensionPoint;
	SharedPtr<CommandProcessor>     _pCommandProcessor;
	SharedPtr<Poco::Net::TCPServer> _pTCPServer;
#if defined(OSP_SHELL_ENABLE_SSH)
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
