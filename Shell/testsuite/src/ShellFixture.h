//
// ShellFixture.h
//
// Definition of the ShellFixture class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ShellFixture_INCLUDED
#define ShellFixture_INCLUDED


#include "TestCommandFactory.h"
#include "CommandExtensionPoint.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/LoginFailures.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/ShellServer.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/AutoPtr.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>


class ShellFixture
	/// An OSP framework with the shell for tests.
	///
	/// The bundles are directories with a unique name below the temporary
	/// directory, loaded and started by a bundle loader that has its own
	/// service registry:
	///   - BASE_BUNDLE, which the shell bundle requires;
	///   - SHELL_BUNDLE, with the bundle properties of the shell bundle;
	///   - COMMANDS_BUNDLE, which requires the shell bundle, with the bundle
	///     properties, extensions and library of the shell commands bundle;
	///   - LOGLEVEL_BUNDLE, which requires the shell bundle, with the bundle
	///     properties, extensions and library of the loglevel command bundle;
	///   - OTHER_BUNDLE, which no other bundle requires.
	///
	/// The libraries of the bundles are in a code cache that all ShellFixture
	/// objects of the process share, so that each library is loaded once.
	///
	/// The service registry has a bundle installer service, with a bundle
	/// repository in an empty directory.
	///
	/// The command processor has the context of the shell bundle. It has the
	/// help, login and quit commands of the shell bundle and the commands of
	/// COMMANDS_BUNDLE and LOGLEVEL_BUNDLE, added through the command extension
	/// point as the shell bundle adds them.
	///
	/// The directory is removed when the ShellFixture is destroyed.
{
public:
	ShellFixture() = delete;

	explicit ShellFixture(const std::string& authServiceName);
		/// Creates the framework. Sessions use the authentication service with
		/// the given name; an empty name disables authentication.

	~ShellFixture();
		/// Stops the shell server and waits for its connections to end, then
		/// destroys the command processor, the bundles and the loader, and
		/// removes the directory.

	ShellFixture(const ShellFixture&) = delete;
	ShellFixture(ShellFixture&&) = delete;
	ShellFixture& operator = (const ShellFixture&) = delete;
	ShellFixture& operator = (ShellFixture&&) = delete;

	[[nodiscard]] Poco::OSP::BundleContext::Ptr shellContext() const;
		/// Returns the context of the shell bundle.

	[[nodiscard]] Poco::OSP::Bundle::Ptr bundle(const std::string& symbolicName) const;
		/// Returns the bundle with the given symbolic name.

	[[nodiscard]] Poco::OSP::ServiceRegistry& registry();
		/// Returns the service registry of the bundle contexts.

	[[nodiscard]] Poco::OSP::Shell::CommandProcessor& processor();
		/// Returns the command processor.

	[[nodiscard]] Poco::OSP::Shell::CommandExtensionPoint& extensionPoint();
		/// Returns the command extension point.

	[[nodiscard]] const std::string& authServiceName() const;
		/// Returns the name of the authentication service of the sessions.

	void addCommand(const std::string& command, const std::string& permission, TestCommandFactory::Function function);
		/// Adds a command that calls the given function to the command processor,
		/// with the given permission and a factory in the context of the shell bundle.

	void addBundle(const std::string& symbolicName, const std::string& requiredBundle,
		const std::string& headers = std::string());
		/// Creates and starts a bundle with the given symbolic name, which
		/// requires the given bundle unless it is empty, with the given
		/// manifest headers (each ending in a newline) added to its manifest.

	void addCommandBundle(const std::string& symbolicName, const std::string& sourceDirectory,
		const std::string& libraryPath);
		/// Creates, starts and adds the commands of a bundle with the given symbolic
		/// name, which requires the shell bundle, with the files of the given
		/// directory (relative to the shell source directory) and the library at
		/// the given path.

	void addExtensions(const std::string& symbolicName);
		/// Passes each extension in the extensions.xml file of the bundle with the
		/// given symbolic name to the command extension point.

	[[nodiscard]] Poco::OSP::Bundle::Ptr addShellBundle(const std::string& symbolicName,
		const std::string& properties);
		/// Creates and starts a bundle with the given symbolic name, which
		/// requires BASE_BUNDLE and runs the activator and the library of the
		/// shell bundle, with the files of the shell bundle and the given bundle
		/// properties added to its bundle properties. The activator starts its
		/// own command processor and shell server.
		///
		/// Registers the extension point service, which the activator needs, so
		/// it can be called once per fixture.

	[[nodiscard]] Poco::OSP::Shell::ShellConnection::Config connectionConfig(std::chrono::seconds sessionTimeout);
		/// Returns the config for shell sessions with the command processor, the
		/// authentication service name and the rejected logins of the fixture.

	[[nodiscard]] Poco::UInt16 startServer(std::chrono::seconds sessionTimeout = DEFAULT_SESSION_TIMEOUT);
		/// Starts a shell server on 127.0.0.1, with a port that the system
		/// chooses and the given session timeout, and returns the port.

	[[nodiscard]] Poco::OSP::Shell::ShellServer& server();
		/// Returns the shell server that startServer() has started.

	[[nodiscard]] std::size_t stopServer(std::chrono::milliseconds timeout);
		/// Stops the shell server and waits until its sessions have ended, at
		/// most for the given time. Returns the number of sessions that have not
		/// ended; the server is then kept until a later stopServer() returns 0.

	void stopServer();
		/// Stops the shell server and waits until its sessions have ended.
		/// Throws a Poco::IllegalStateException if they do not end within
		/// ten seconds.

	[[nodiscard]] static Poco::UInt16 freePort();
		/// Returns a port on 127.0.0.1 that is free at the moment, for servers
		/// that cannot report a port that the system chooses.

	static constexpr std::chrono::seconds DEFAULT_SESSION_TIMEOUT{600};

	static constexpr std::chrono::milliseconds FIRST_LOGIN_DELAY{50};
		/// The delay after the first rejected login, short so that the tests run fast.

	static const std::string BASE_BUNDLE;
	static const std::string SHELL_BUNDLE;
	static const std::string COMMANDS_BUNDLE;
	static const std::string LOGLEVEL_BUNDLE;
	static const std::string OTHER_BUNDLE;

private:
	void createBundle(const std::string& symbolicName, const std::string& requiredBundle,
		const std::string& sourceDirectory,
		const std::string& activator = std::string(), const std::string& properties = std::string(),
		const std::string& headers = std::string());
		/// Creates the directory bundle with the given symbolic name, which requires
		/// the given bundle unless it is empty, with the files of the given directory
		/// (relative to the shell source directory) unless it is empty.
		///
		/// A non-empty activator is the class name of the bundle activator, and
		/// non-empty properties are added to the bundle.properties file. The given
		/// manifest headers are added to the manifest.

	void installLibrary(const std::string& symbolicName, const std::string& path);
		/// Copies the library at the given path to the code cache, as the library
		/// of the bundle with the given symbolic name, unless the code cache has
		/// this library already.

	std::string _authServiceName;
	Poco::TemporaryFile _directory;
	Poco::OSP::SystemEvents _systemEvents;
	Poco::OSP::ServiceRegistry _registry;
	std::unique_ptr<Poco::OSP::CodeCache> _pCodeCache;
	Poco::OSP::BundleContextFactory::Ptr _pBundleContextFactory;
	std::unique_ptr<Poco::OSP::BundleLoader> _pLoader;
	std::unique_ptr<Poco::OSP::BundleRepository> _pRepository;
	Poco::OSP::BundleContext::Ptr _pShellContext;
	Poco::AutoPtr<Poco::OSP::Shell::CommandExtensionPoint> _pExtensionPoint;
	std::unique_ptr<Poco::OSP::Shell::CommandProcessor> _pProcessor;
	Poco::OSP::Shell::LoginFailures _loginFailures{FIRST_LOGIN_DELAY};
	std::unique_ptr<Poco::OSP::Shell::ShellServer> _pServer;
};


//
// inlines
//
inline Poco::OSP::BundleContext::Ptr ShellFixture::shellContext() const
{
	return _pShellContext;
}


inline Poco::OSP::ServiceRegistry& ShellFixture::registry()
{
	return _registry;
}


inline Poco::OSP::Shell::CommandProcessor& ShellFixture::processor()
{
	return *_pProcessor;
}


inline Poco::OSP::Shell::CommandExtensionPoint& ShellFixture::extensionPoint()
{
	return *_pExtensionPoint;
}


inline const std::string& ShellFixture::authServiceName() const
{
	return _authServiceName;
}


inline Poco::OSP::Shell::ShellServer& ShellFixture::server()
{
	return *_pServer;
}


#endif // ShellFixture_INCLUDED
