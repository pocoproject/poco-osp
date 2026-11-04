//
// BundleActivator.cpp
//
// Copyright (c) 2016, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/RemotingNG/ORB.h"
#include "Poco/RemotingNG/RemoteObject.h"
#include "Poco/RemotingNG/TCP/Listener.h"
#include "Poco/RemotingNG/TCP/Transport.h"
#include "Poco/RemotingNG/TCP/TransportFactory.h"
#include "Poco/RemotingNG/TCP/ConnectionManager.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Ascii.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include "Poco/Mutex.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/Timespan.h"
#include "Poco/ClassLibrary.h"
#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <string>
#if defined(POCO_OS_FAMILY_UNIX)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif


namespace Poco::OSP::RemotingNG::TCP {


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	BundleActivator() = default;

	~BundleActivator() override = default;

	void start(Poco::OSP::BundleContext::Ptr pContext) override
	{
		{
			// The event handlers read it under the lock.
			Poco::FastMutex::ScopedLock lock(_mutex);
			_pContext = pContext;
		}
		_pPrefs = Poco::OSP::ServiceFinder::find<Poco::OSP::PreferencesService>(_pContext);

		std::string endpoint = _pPrefs->configuration()->getString("remoting.tcp.listener", "");
		if (!endpoint.empty())
		{
			// Read before anything is registered: the framework does not call stop() when start() fails.
			const Poco::Timespan idleTimeout(
				_pPrefs->configuration()->getInt("remoting.tcp.connection.idleTimeout", 60), 0);
			const Poco::Timespan handshakeTimeout(
				_pPrefs->configuration()->getInt("remoting.tcp.connection.handshakeTimeout", 8), 0);

			Poco::RemotingNG::TCP::Listener::Ptr pListener;
#if defined(POCO_OS_FAMILY_UNIX)
			if (endpoint[0] == '/')
			{
				pListener = createLocalListener(endpoint, _pPrefs->configuration()->getString("remoting.tcp.listener.mode", "0660"));
			}
			else
#endif
			{
				pListener = createTCPListener(endpoint);
			}
			pListener->setHandshakeTimeout(handshakeTimeout);
			Poco::RemotingNG::TCP::ConnectionManager::defaultManager().setIdleTimeout(idleTimeout);
			Poco::RemotingNG::TCP::ConnectionManager::defaultManager().setHandshakeTimeout(handshakeTimeout);
			_listener = Poco::RemotingNG::ORB::instance().registerListener(pListener);

			_pContext->logger().information("Remoting TCP Listener ready: %s", _listener);

			// Subscribed before the services are listed, so that no service registered in between is missed.
			pContext->registry().serviceRegistered   += Poco::delegate(this, &BundleActivator::handleServiceRegistered);
			pContext->registry().serviceUnregistered += Poco::delegate(this, &BundleActivator::handleServiceUnregistered);

			std::vector<ServiceRef::Ptr> services;
			pContext->registry().find("name", services);
			for (std::vector<ServiceRef::Ptr>::iterator it = services.begin(); it != services.end(); ++it)
			{
				registerServiceWithORB(*it);
			}
		}

		// Registered last: the framework does not call stop() when start() fails.
		Poco::RemotingNG::TCP::TransportFactory::registerFactory();
	}

	void stop(BundleContext::Ptr pContext) override
	{
		_pPrefs = nullptr;

		if (!_listener.empty())
		{
			Poco::RemotingNG::ORB::instance().unregisterListener(_listener, true);

			pContext->registry().serviceRegistered   -= Poco::delegate(this, &BundleActivator::handleServiceRegistered);
			pContext->registry().serviceUnregistered -= Poco::delegate(this, &BundleActivator::handleServiceUnregistered);

			// An event handler can still run: it reads the members under the lock.
			Poco::FastMutex::ScopedLock lock(_mutex);
			_listener.clear();
			_exportedServices.clear();
			_removedNames.clear();
		}

		Poco::RemotingNG::TCP::TransportFactory::unregisterFactory();

		Poco::FastMutex::ScopedLock lock(_mutex);
		_pContext = nullptr;
	}

	void handleServiceRegistered(const void* /*sender*/, ServiceEvent& event)
	{
		Poco::OSP::BundleContext::Ptr pContext;
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			pContext = _pContext;
		}
		if (pContext == nullptr) return;
		pContext->logger().debug("Service registered: " + event.service()->name());
		registerServiceWithORB(event.service());
	}

	void handleServiceUnregistered(const void* /*sender*/, ServiceEvent& event)
	{
		Poco::OSP::BundleContext::Ptr pContext;
		std::string uri;
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			ExportedServices::iterator it = _exportedServices.find(event.service()->name());
			if (it == _exportedServices.end() || it->second.pServiceRef != event.service()) return;
			uri = it->second.uri;
			_exportedServices.erase(it);
			_removedNames.insert(event.service()->name());
			pContext = _pContext;
		}
		// While the service is being exported, registerServiceWithORB() removes its object.
		if (!uri.empty()) unregisterServiceFromORB(pContext, event.service(), uri);
	}

	void registerServiceWithORB(ServiceRef::Ptr pServiceRef)
		/// Exports the service if it is enabled for the TCP transport and not yet exported.
		/// Errors are logged: they must not fail the registration of the service.
		/// The code of the service (a service factory, the remote object) runs
		/// without the lock, so that it can register and unregister services.
	{
		const std::string& name = pServiceRef->name();
		Poco::OSP::BundleContext::Ptr pContext;
		std::string listener;
		{
			// The services exported by an earlier start() still have the property
			// remoting.tcp.uri, so the exported services are recorded here.
			Poco::FastMutex::ScopedLock lock(_mutex);
			if (_pContext == nullptr) return;
			// A service can be unregistered before its registration event or the
			// listing of start() reaches this function. The registry removes the
			// service before it reports the unregistration, so either this check
			// or handleServiceUnregistered() sees the change.
			if (_pContext->registry().findByName(name) != pServiceRef.get()) return;
			if (!pServiceRef->properties().getBool("remoting.tcp.enable", false)) return;
			// Recorded before the export, so that the listing of start() and the
			// registration event do not both export the service. While a service
			// unregistered under the same name is still recorded or its object is
			// being removed, that object can have the URI of this service: the
			// removal exports this service afterwards.
			if (_exportedServices.contains(name) || _removedNames.contains(name)) return;
			_exportedServices[name] = ExportedService{pServiceRef, std::string()};
			pContext = _pContext;
			listener = _listener;
		}

		std::string uri;
		bool exported = false;
		try
		{
			Poco::OSP::Service::Ptr pService = pServiceRef->instance();
			if (dynamic_cast<Poco::RemotingNG::RemoteObject*>(pService.get()) != nullptr)
			{
				Poco::RemotingNG::RemoteObject::Ptr pRemoteObject = pService.cast<Poco::RemotingNG::RemoteObject>();
				uri = Poco::RemotingNG::ORB::instance().registerObject(pRemoteObject, listener);
				pRemoteObject->remoting__enableRemoteEvents(Poco::RemotingNG::TCP::Transport::PROTOCOL);
				exported = true;
			}
		}
		catch (Poco::Exception& exc)
		{
			pContext->logger().error("Failed to export the service %s with the Remoting TCP transport: %s", name, exc.displayText());
		}
		catch (std::exception& exc)
		{
			pContext->logger().error("Failed to export the service %s with the Remoting TCP transport: %s", name, std::string(exc.what()));
		}
		catch (...)
		{
			pContext->logger().error("Failed to export the service %s with the Remoting TCP transport: unknown exception", name);
		}

		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			ExportedServices::iterator it = _exportedServices.find(name);
			const bool recorded = it != _exportedServices.end() && it->second.pServiceRef == pServiceRef;
			if (exported && recorded)
			{
				it->second.uri = uri;
				pServiceRef->properties().set("remoting.tcp.uri", uri);
				pContext->logger().information("Registered service with ORB: %s", uri);
				return;
			}
			// The export failed, or the service was unregistered during the export.
			if (recorded) _exportedServices.erase(it);
			_removedNames.insert(name);
		}
		unregisterServiceFromORB(pContext, pServiceRef, uri);
	}

	void unregisterServiceFromORB(const Poco::OSP::BundleContext::Ptr& pContext, const ServiceRef::Ptr& pServiceRef, const std::string& uri)
		/// Removes the object of the service with the given URI from the ORB,
		/// unless the URI is empty, then exports the service registered under
		/// the same name since. The name of the service must be in _removedNames.
		/// Errors are logged.
	{
		const std::string& name = pServiceRef->name();
		if (!uri.empty())
		{
			pContext->logger().information("Unregistering service with ORB: %s", name);
			try
			{
				Poco::RemotingNG::ORB::instance().unregisterObject(uri);
			}
			catch (Poco::Exception& exc)
			{
				pContext->logger().error("Failed to remove the service %s from the ORB: %s", name, exc.displayText());
			}
		}
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			_removedNames.erase(name);
		}
		ServiceRef::Ptr pCurrent = pContext->registry().findByName(name);
		if (pCurrent != nullptr && pCurrent != pServiceRef) registerServiceWithORB(pCurrent);
	}

	[[nodiscard]] static Poco::RemotingNG::TCP::Listener::Ptr createTCPListener(const std::string& endpoint)
		/// Creates the listener on a TCP socket at the given <host>:<port>. With
		/// port 0, the system chooses the port, which the URIs of the exported
		/// services then carry. Throws a Poco::InvalidArgumentException for an
		/// endpoint in another form, which those URIs cannot carry.
	{
		// The URIs carry the port as configured, so it must be a number. On Windows,
		// SocketAddress takes a path as the address of a local socket.
		const std::string port(endpoint.substr(endpoint.rfind(':') + 1));
		bool valid = !Poco::Net::SocketAddress::isUnixLocal(endpoint) && !port.empty() &&
			std::all_of(port.begin(), port.end(), [](char c) { return Poco::Ascii::isDigit(c); });
		Poco::Net::SocketAddress address;
		try
		{
			if (valid) address = Poco::Net::SocketAddress(endpoint);
		}
		catch (Poco::InvalidArgumentException&)
		{
			valid = false;
		}
		catch (Poco::Net::ServiceNotFoundException&)
		{
			valid = false;
		}
		if (!valid)
		{
			throw Poco::InvalidArgumentException(
				"remoting.tcp.listener must be <host>:<port>, or on POSIX systems the path of a local socket",
				endpoint);
		}
		const Poco::Net::ServerSocket socket(address);
		std::string uriEndpoint(endpoint);
		if (address.port() == 0)
		{
			uriEndpoint.replace(uriEndpoint.rfind(':') + 1, std::string::npos,
				Poco::NumberFormatter::format(socket.address().port()));
		}
		return new Poco::RemotingNG::TCP::Listener(uriEndpoint, socket, nullptr);
	}

#if defined(POCO_OS_FAMILY_UNIX)
	[[nodiscard]] Poco::RemotingNG::TCP::Listener::Ptr createLocalListener(const std::string& path, const std::string& mode)
		/// Creates the listener on a local (UNIX domain) socket at the given path,
		/// with the given file mode (octal). A socket file that exists at the path,
		/// left by an earlier run, is removed; any other file is kept, and the
		/// listener cannot be created.
	{
		unsigned fileMode = 0;
		if (!Poco::NumberParser::tryParseOct(mode, fileMode))
		{
			throw Poco::InvalidArgumentException("remoting.tcp.listener.mode must be an octal file mode", mode);
		}
		struct stat st{};
		if (::lstat(path.c_str(), &st) == 0 && S_ISSOCK(st.st_mode))
		{
			_pContext->logger().notice("Removing existing socket file %s", path);
			::unlink(path.c_str());
		}
		Poco::Net::ServerSocket socket;
		socket.bind(Poco::Net::SocketAddress(path));
		// chmod() follows a symbolic link put at the path after bind(). Only the type
		// is checked: fstat() of the socket does not return the inode of the socket file.
		if (::lstat(path.c_str(), &st) != 0 || !S_ISSOCK(st.st_mode))
		{
			throw Poco::FileException("The socket file was replaced before its mode was set", path);
		}
		// Set before listen(), so that no client connects with the mode of the umask.
		if (::chmod(path.c_str(), static_cast<mode_t>(fileMode)) != 0)
		{
			throw Poco::FileAccessDeniedException("Cannot set the mode of the socket file", path);
		}
		socket.listen();
		return new Poco::RemotingNG::TCP::Listener(path, socket, nullptr);
	}
#endif

private:
	Poco::OSP::BundleContext::Ptr _pContext;
	Poco::OSP::PreferencesService::Ptr _pPrefs;
	std::string _listener;
	struct ExportedService
	{
		ServiceRef::Ptr pServiceRef;
		std::string uri;
			/// Empty while the service is being exported.
	};
	using ExportedServices = std::map<std::string, ExportedService>;

	ExportedServices _exportedServices;
		/// The services exported with the listener, and those being exported,
		/// by name.
	std::set<std::string> _removedNames;
		/// The names of the services whose object is being removed from the ORB.
	Poco::FastMutex _mutex;
};


} // namespace Poco::OSP::RemotingNG::TCP


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::RemotingNG::TCP::BundleActivator)
POCO_END_MANIFEST
