//
// TCPBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TCPBundleActivatorTest.h"
#include "TemporaryBundle.h"
#include "LogCapture.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceEvent.h"
#include "Poco/OSP/ServiceFactory.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/RemotingNG/Identifiable.h"
#include "Poco/RemotingNG/ORB.h"
#include "Poco/RemotingNG/RemoteObject.h"
#include "Poco/RemotingNG/RemotingException.h"
#include "Poco/RemotingNG/Serializer.h"
#include "Poco/RemotingNG/SerializerBase.h"
#include "Poco/RemotingNG/Deserializer.h"
#include "Poco/RemotingNG/Skeleton.h"
#include "Poco/RemotingNG/TCP/ConnectionManager.h"
#include "Poco/RemotingNG/TCP/Transport.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Channel.h"
#include "Poco/ClassLoader.h"
#include "Poco/Delegate.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/ThreadPool.h"
#include "Poco/URI.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>
#if defined(POCO_OS_FAMILY_UNIX)
#include <sys/stat.h>
#endif


using Poco::OSP::PreferencesService;
using namespace std::string_literals;


namespace
{
	const Poco::RemotingNG::Identifiable::TypeId UNKNOWN_TYPE("StubRemoteService"s);
		/// A type for which no skeleton is registered.

	const Poco::RemotingNG::Identifiable::TypeId EXPORTED_TYPE("ExportedRemoteService"s);
		/// A type for which the fixture registers a skeleton.

	constexpr long WAIT_MILLISECONDS = 10000;

	class StubRemoteService: public Poco::OSP::Service, public Poco::RemotingNG::RemoteObject
		/// A remote service of the given type, with the given object ID.
	{
	public:
		StubRemoteService() = delete;

		explicit StubRemoteService(const Poco::RemotingNG::Identifiable::TypeId& typeId, const std::string& objectId = "stub"s):
			Poco::RemotingNG::RemoteObject(objectId),
			_typeId(typeId)
		{
		}

		~StubRemoteService() override = default;

		StubRemoteService(const StubRemoteService&) = delete;
		StubRemoteService(StubRemoteService&&) = delete;
		StubRemoteService& operator = (const StubRemoteService&) = delete;
		StubRemoteService& operator = (StubRemoteService&&) = delete;

		[[nodiscard]] const Poco::RemotingNG::Identifiable::TypeId& remoting__typeId() const override
		{
			return _typeId;
		}

		const std::type_info& type() const override
		{
			return typeid(StubRemoteService);
		}

		bool isA(const std::type_info& otherType) const override
		{
			return otherType == typeid(StubRemoteService) || Poco::OSP::Service::isA(otherType);
		}

	private:
		const Poco::RemotingNG::Identifiable::TypeId _typeId;
	};

	class Unregisterer
		/// Unregisters every service as soon as it is registered, before the
		/// delegates subscribed after it are called, like another thread that
		/// unregisters the service while the registration is reported.
	{
	public:
		Unregisterer() = delete;

		explicit Unregisterer(Poco::OSP::ServiceRegistry& registry):
			_registry(registry)
		{
			_registry.serviceRegistered += Poco::delegate(this, &Unregisterer::handleServiceRegistered);
		}

		~Unregisterer()
		{
			try
			{
				_registry.serviceRegistered -= Poco::delegate(this, &Unregisterer::handleServiceRegistered);
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		Unregisterer(const Unregisterer&) = delete;
		Unregisterer(Unregisterer&&) = delete;
		Unregisterer& operator = (const Unregisterer&) = delete;
		Unregisterer& operator = (Unregisterer&&) = delete;

	private:
		void handleServiceRegistered(const void*, Poco::OSP::ServiceEvent& event)
		{
			_registry.unregisterService(event.service()->name());
		}

		Poco::OSP::ServiceRegistry& _registry;
	};

	Poco::OSP::ServiceRef::Ptr registerRemoteService(Poco::OSP::ServiceRegistry& registry, const std::string& name)
		/// Registers a remote service of EXPORTED_TYPE, enabled for the TCP
		/// transport, with the given name, which is also its object ID.
	{
		Poco::OSP::Properties properties;
		properties.set("remoting.tcp.enable"s, true);
		return registry.registerService(name, new StubRemoteService(EXPORTED_TYPE, name), properties);
	}

	class Reregisterer
		/// Registers a remote service under the name of the first service whose
		/// unregistration is reported, before the delegates subscribed after it
		/// are called, like another thread that registers the service again as
		/// soon as the registry has removed the old one.
	{
	public:
		Reregisterer() = delete;

		explicit Reregisterer(Poco::OSP::ServiceRegistry& registry):
			_registry(registry)
		{
			_registry.serviceUnregistered += Poco::delegate(this, &Reregisterer::handleServiceUnregistered);
		}

		~Reregisterer()
		{
			try
			{
				_registry.serviceUnregistered -= Poco::delegate(this, &Reregisterer::handleServiceUnregistered);
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		Reregisterer(const Reregisterer&) = delete;
		Reregisterer(Reregisterer&&) = delete;
		Reregisterer& operator = (const Reregisterer&) = delete;
		Reregisterer& operator = (Reregisterer&&) = delete;

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr service() const
			/// Returns the service registered again, or null.
		{
			return _pService;
		}

	private:
		void handleServiceUnregistered(const void*, Poco::OSP::ServiceEvent& event)
		{
			if (_pService == nullptr) _pService = registerRemoteService(_registry, event.service()->name());
		}

		Poco::OSP::ServiceRegistry& _registry;
		Poco::OSP::ServiceRef::Ptr _pService;
	};

	class RegisteringFactory: public Poco::OSP::ServiceFactory
		/// A service factory whose createService() registers the remote service
		/// osp.test.created the first time it is called.
	{
	public:
		RegisteringFactory() = delete;

		explicit RegisteringFactory(Poco::OSP::ServiceRegistry& registry):
			_registry(registry)
		{
		}

		~RegisteringFactory() override = default;

		RegisteringFactory(const RegisteringFactory&) = delete;
		RegisteringFactory(RegisteringFactory&&) = delete;
		RegisteringFactory& operator = (const RegisteringFactory&) = delete;
		RegisteringFactory& operator = (RegisteringFactory&&) = delete;

		Poco::OSP::Service::Ptr createService() override
		{
			if (_pCreated == nullptr) _pCreated = registerRemoteService(_registry, "osp.test.created"s);
			return new StubRemoteService(EXPORTED_TYPE, "factory"s);
		}

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr created() const
			/// Returns the service registered by createService(), or null.
		{
			return _pCreated;
		}

	private:
		Poco::OSP::ServiceRegistry& _registry;
		Poco::OSP::ServiceRef::Ptr _pCreated;
	};

	class RegisteringRemoteService: public StubRemoteService
		/// A remote service whose remoting__enableRemoteEvents() registers the
		/// remote service osp.test.registered the first time it is called.
	{
	public:
		RegisteringRemoteService() = delete;

		explicit RegisteringRemoteService(Poco::OSP::ServiceRegistry& registry):
			StubRemoteService(EXPORTED_TYPE),
			_registry(registry)
		{
		}

		~RegisteringRemoteService() override = default;

		RegisteringRemoteService(const RegisteringRemoteService&) = delete;
		RegisteringRemoteService(RegisteringRemoteService&&) = delete;
		RegisteringRemoteService& operator = (const RegisteringRemoteService&) = delete;
		RegisteringRemoteService& operator = (RegisteringRemoteService&&) = delete;

		void remoting__enableRemoteEvents(const std::string&) override
		{
			if (_pRegistered == nullptr) _pRegistered = registerRemoteService(_registry, "osp.test.registered"s);
		}

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr registered() const
			/// Returns the service registered by remoting__enableRemoteEvents(), or null.
		{
			return _pRegistered;
		}

	private:
		Poco::OSP::ServiceRegistry& _registry;
		Poco::OSP::ServiceRef::Ptr _pRegistered;
	};

	class PausingChannel: public Poco::Channel
		/// Sets the given event at the first error message, and stops the thread
		/// that logs it until resume() is called, like a thread that is preempted
		/// at that point.
	{
	public:
		using Ptr = Poco::AutoPtr<PausingChannel>;

		PausingChannel() = delete;

		explicit PausingChannel(Poco::Event& paused):
			_paused(paused)
		{
		}

		PausingChannel(const PausingChannel&) = delete;
		PausingChannel(PausingChannel&&) = delete;
		PausingChannel& operator = (const PausingChannel&) = delete;
		PausingChannel& operator = (PausingChannel&&) = delete;

		void log(const Poco::Message& msg) override
		{
			if (msg.getPriority() <= Poco::Message::PRIO_ERROR && !_hasPaused.exchange(true))
			{
				_paused.set();
				static_cast<void>(_resumed.tryWait(WAIT_MILLISECONDS));
			}
		}

		void resume()
		{
			_resumed.set();
		}

	protected:
		~PausingChannel() override = default;
			/// Reference counted: destroyed through release() only.

	private:
		Poco::Event& _paused;
		Poco::Event _resumed;
		std::atomic<bool> _hasPaused{false};
	};

	class ConcurrentReregisterer
		/// When the first service is unregistered, registers a remote service under
		/// its name in another thread, before the delegates subscribed after it are
		/// called, and waits until that thread has finished or has logged an error.
		/// At its first error message, the other thread waits until resume() is
		/// called, like a thread that is preempted at that point.
	{
	public:
		ConcurrentReregisterer() = delete;

		ConcurrentReregisterer(Poco::OSP::ServiceRegistry& registry, Poco::Logger& logger):
			_registry(registry),
			_logger(logger),
			_pPreviousChannel(logger.getChannel()),
			_pChannel(new PausingChannel(_waiting))
		{
			_logger.setChannel(_pChannel);
			_registry.serviceUnregistered += Poco::delegate(this, &ConcurrentReregisterer::handleServiceUnregistered);
		}

		~ConcurrentReregisterer()
		{
			try
			{
				_registry.serviceUnregistered -= Poco::delegate(this, &ConcurrentReregisterer::handleServiceUnregistered);
				resume();
				_logger.setChannel(_pPreviousChannel);
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		ConcurrentReregisterer(const ConcurrentReregisterer&) = delete;
		ConcurrentReregisterer(ConcurrentReregisterer&&) = delete;
		ConcurrentReregisterer& operator = (const ConcurrentReregisterer&) = delete;
		ConcurrentReregisterer& operator = (ConcurrentReregisterer&&) = delete;

		void resume()
			/// Lets the other thread go on and waits until it has finished.
		{
			_pChannel->resume();
			if (_thread.joinable()) _thread.join();
		}

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr service() const
			/// Returns the service registered again, or null. Valid after resume().
		{
			return _pService;
		}

	private:
		void handleServiceUnregistered(const void*, Poco::OSP::ServiceEvent& event)
		{
			if (_started) return;
			_started = true;
			const std::string name = event.service()->name();
			_thread = std::thread([this, name]()
				{
					_pService = registerRemoteService(_registry, name);
					_waiting.set();
				});
			static_cast<void>(_waiting.tryWait(WAIT_MILLISECONDS));
		}

		Poco::OSP::ServiceRegistry& _registry;
		Poco::Logger& _logger;
		Poco::Channel::Ptr _pPreviousChannel;
		Poco::Event _waiting;
		PausingChannel::Ptr _pChannel;
		std::thread _thread;
		bool _started = false;
		Poco::OSP::ServiceRef::Ptr _pService;
	};

	class SwappingFactory: public Poco::OSP::ServiceFactory
		/// A service factory whose first createService() unregisters the service
		/// of the factory and registers a remote service under its name.
	{
	public:
		SwappingFactory() = delete;

		explicit SwappingFactory(Poco::OSP::ServiceRegistry& registry):
			_registry(registry)
		{
		}

		~SwappingFactory() override = default;

		SwappingFactory(const SwappingFactory&) = delete;
		SwappingFactory(SwappingFactory&&) = delete;
		SwappingFactory& operator = (const SwappingFactory&) = delete;
		SwappingFactory& operator = (SwappingFactory&&) = delete;

		void setService(const Poco::OSP::ServiceRef::Ptr& pService)
			/// Sets the service of the factory.
		{
			_pService = pService;
		}

		Poco::OSP::Service::Ptr createService() override
		{
			if (_pService != nullptr)
			{
				// Released here: the service holds the factory.
				const Poco::OSP::ServiceRef::Ptr pService = _pService;
				_pService = nullptr;
				_registry.unregisterService(pService);
				_pOther = registerRemoteService(_registry, pService->name());
			}
			return new StubRemoteService(EXPORTED_TYPE, "factory"s);
		}

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr other() const
			/// Returns the service registered by createService(), or null.
		{
			return _pOther;
		}

	private:
		Poco::OSP::ServiceRegistry& _registry;
		Poco::OSP::ServiceRef::Ptr _pService;
		Poco::OSP::ServiceRef::Ptr _pOther;
	};

	class ChannelGuard
		/// Sends the messages of the logger, from the information level up, to
		/// the given channel while the object exists.
	{
	public:
		ChannelGuard() = delete;

		ChannelGuard(Poco::Logger& logger, const Poco::Channel::Ptr& pChannel):
			_logger(logger),
			_pPreviousChannel(logger.getChannel()),
			_previousLevel(logger.getLevel())
		{
			_logger.setChannel(pChannel);
			_logger.setLevel(Poco::Message::PRIO_INFORMATION);
		}

		~ChannelGuard()
		{
			try
			{
				_logger.setChannel(_pPreviousChannel);
				_logger.setLevel(_previousLevel);
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		ChannelGuard(const ChannelGuard&) = delete;
		ChannelGuard(ChannelGuard&&) = delete;
		ChannelGuard& operator = (const ChannelGuard&) = delete;
		ChannelGuard& operator = (ChannelGuard&&) = delete;

	private:
		Poco::Logger& _logger;
		const Poco::Channel::Ptr _pPreviousChannel;
		const int _previousLevel;
	};

	class RegisteringChannel: public Poco::Channel
		/// Counts the error messages. At the first message that reports the
		/// removal of the service osp.test.remote from the ORB, registers a remote
		/// service under that name, with the same object ID, like another thread
		/// that registers the service again while the old object is removed.
	{
	public:
		using Ptr = Poco::AutoPtr<RegisteringChannel>;

		RegisteringChannel() = delete;

		explicit RegisteringChannel(Poco::OSP::ServiceRegistry& registry):
			_registry(registry)
		{
		}

		RegisteringChannel(const RegisteringChannel&) = delete;
		RegisteringChannel(RegisteringChannel&&) = delete;
		RegisteringChannel& operator = (const RegisteringChannel&) = delete;
		RegisteringChannel& operator = (RegisteringChannel&&) = delete;

		void log(const Poco::Message& msg) override
		{
			if (msg.getPriority() <= Poco::Message::PRIO_ERROR) ++_errors;
			if (_pService == nullptr && msg.getText().find("Unregistering service with ORB: osp.test.remote"s) != std::string::npos)
			{
				_pService = registerRemoteService(_registry, "osp.test.remote"s);
			}
		}

		[[nodiscard]] Poco::OSP::ServiceRef::Ptr service() const
			/// Returns the service registered again, or null.
		{
			return _pService;
		}

		[[nodiscard]] int errors() const
		{
			return _errors;
		}

	protected:
		~RegisteringChannel() override = default;
			/// Reference counted: destroyed through release() only.

	private:
		Poco::OSP::ServiceRegistry& _registry;
		Poco::OSP::ServiceRef::Ptr _pService;
		int _errors = 0;
	};

	class ThrowingFactory: public Poco::OSP::ServiceFactory
		/// A service factory whose createService() throws a std::exception.
	{
	public:
		ThrowingFactory() = default;

		~ThrowingFactory() override = default;

		ThrowingFactory(const ThrowingFactory&) = delete;
		ThrowingFactory(ThrowingFactory&&) = delete;
		ThrowingFactory& operator = (const ThrowingFactory&) = delete;
		ThrowingFactory& operator = (ThrowingFactory&&) = delete;

		Poco::OSP::Service::Ptr createService() override
		{
			throw std::runtime_error("no service");
		}
	};

	bool isExported(const Poco::OSP::ServiceRef::Ptr& pRef)
		/// Returns true if the ORB has the service object under the URI in the
		/// property remoting.tcp.uri of the service.
	{
		const std::string uri = pRef->properties().get("remoting.tcp.uri"s, ""s);
		if (uri.empty()) return false;
		try
		{
			return Poco::RemotingNG::ORB::instance().findObject(uri).get() == dynamic_cast<Poco::RemotingNG::Identifiable*>(pRef->instance().get());
		}
		catch (Poco::RemotingNG::UnknownTypeException&)
		{
			// Without a local object, the ORB looks for a proxy factory for the type.
			return false;
		}
	}

	bool skeletonAnswers(const std::string& uri)
		/// Sends a request for a method that the skeleton of EXPORTED_TYPE does
		/// not have to the StubRemoteService "stub" at the URI, over a TCP
		/// connection, and returns true if the skeleton answered it. The listener
		/// answers a request for an unknown object with another fault.
	{
		using Poco::RemotingNG::SerializerBase;

		const Poco::RemotingNG::Identifiable::TypeId& typeId = EXPORTED_TYPE;
		const Poco::RemotingNG::Identifiable::ObjectId objectId("stub"s);
		const std::string method("unknownMethod"s);
		Poco::RemotingNG::TCP::Transport transport(Poco::RemotingNG::TCP::ConnectionManager::defaultManager());
		transport.connect(uri);
		Poco::RemotingNG::Serializer& serializer =
			transport.beginRequest(objectId, typeId, method, SerializerBase::MESSAGE_REQUEST);
		serializer.serializeMessageBegin(method, SerializerBase::MESSAGE_REQUEST);
		serializer.serializeMessageEnd(method, SerializerBase::MESSAGE_REQUEST);
		Poco::RemotingNG::Deserializer& deserializer =
			transport.sendRequest(objectId, typeId, method, SerializerBase::MESSAGE_REQUEST);
		bool answered = false;
		try
		{
			std::string name;
			static_cast<void>(deserializer.findMessage(name));
		}
		catch (Poco::RemotingNG::RemoteException& exc)
		{
			// A fault carries the name of the exception of the server, not its type.
			answered = exc.message().find(Poco::RemotingNG::MethodNotFoundException().name()) == 0;
		}
		transport.endRequest();
		return answered;
	}
}


struct TCPBundleActivatorTest::Fixture
	/// The bundle activator of the bundle library, with a bundle context in
	/// whose registry the preferences service is registered, a directory
	/// for the socket file, and a skeleton for EXPORTED_TYPE.
{
	Fixture():
		bundles(""s),
		pSettings(new Poco::Util::MapConfiguration),
		pPreferences(new PreferencesService(Poco::Path(persistency.path()), pSettings))
	{
		Poco::File(directory.path()).createDirectories();
		socketPath = Poco::Path(Poco::Path(directory.path()).makeDirectory(), "remoting.sock"s).toString();
		bundles.registry().registerService(PreferencesService::SERVICE_NAME, pPreferences, Poco::OSP::Properties());
		loader.loadLibrary(TCP_BUNDLE_LIBRARY);
		pActivator.reset(loader.create("Poco::OSP::RemotingNG::TCP::BundleActivator"s));
#if defined(POCO_OS_FAMILY_UNIX)
		pSettings->setString("remoting.tcp.listener"s, socketPath);
#else
		// A local socket is configured with a path that starts with "/", which no Windows path does.
		pSettings->setString("remoting.tcp.listener"s, "127.0.0.1:0"s);
#endif
		Poco::RemotingNG::ORB::instance().registerSkeleton(EXPORTED_TYPE, new Poco::RemotingNG::Skeleton);
	}

	~Fixture()
	{
		try
		{
			Poco::RemotingNG::ORB::instance().unregisterSkeleton(EXPORTED_TYPE, true);
		}
		catch (...)
		{
			poco_unexpected();
		}
	}

	Fixture(const Fixture&) = delete;
	Fixture(Fixture&&) = delete;
	Fixture& operator = (const Fixture&) = delete;
	Fixture& operator = (Fixture&&) = delete;

	void start()
	{
		pActivator->start(bundles.context());
		started = true;
	}

	void stop()
	{
		started = false;
		pActivator->stop(bundles.context());
	}

	TemporaryBundle bundles;
	Poco::TemporaryFile persistency;
	Poco::TemporaryFile directory;
	std::string socketPath;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings;
	PreferencesService::Ptr pPreferences;
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	// Declared after the loader, so that it is destroyed before the library is unloaded.
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator;
	bool started = false;
};


TCPBundleActivatorTest::TCPBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


TCPBundleActivatorTest::~TCPBundleActivatorTest() = default;


#if defined(POCO_OS_FAMILY_UNIX)


void TCPBundleActivatorTest::testRegularFileAtSocketPathIsKept()
{
	{
		Poco::FileOutputStream ostr(_pFixture->socketPath);
		ostr << "data";
	}

	try
	{
		_pFixture->start();
		fail("a regular file is in the place of the socket file");
	}
	catch (Poco::IOException&)
	{
	}

	assertTrue (Poco::File(_pFixture->socketPath).isFile());
	assertEqual (4, static_cast<int>(Poco::File(_pFixture->socketPath).getSize()));
}


void TCPBundleActivatorTest::testSocketFileHasConfiguredMode()
{
	_pFixture->pSettings->setString("remoting.tcp.listener.mode"s, "0600"s);

	_pFixture->start();

	struct stat st{};
	assertEqual (0, ::stat(_pFixture->socketPath.c_str(), &st));
	assertTrue (S_ISSOCK(st.st_mode));
	assertEqual (0600, static_cast<int>(st.st_mode & 0777));
	_pFixture->stop();
}


#endif // POCO_OS_FAMILY_UNIX


void TCPBundleActivatorTest::testExportErrorDoesNotFailRegistration()
{
	_pFixture->start();
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new StubRemoteService(UNKNOWN_TYPE), properties);

	assertTrue (!pRef->properties().has("remoting.tcp.uri"s));
	_pFixture->bundles.registry().unregisterService(pRef);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testListenerOnPortZeroExportsChosenPort()
{
	_pFixture->pSettings->setString("remoting.tcp.listener"s, "127.0.0.1:0"s);
	_pFixture->start();
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService(
		"osp.test.remote"s, new StubRemoteService(EXPORTED_TYPE), properties);

	const std::string uri = pRef->properties().get("remoting.tcp.uri"s, ""s);
	assertTrue (Poco::URI(uri).getPort() != 0);
	assertTrue (skeletonAnswers(uri));
	_pFixture->bundles.registry().unregisterService(pRef);
	_pFixture->stop();
	// The connection manager keeps the connection of the request open; its
	// threads end once it is closed, so that none outlives the test.
	Poco::RemotingNG::TCP::ConnectionManager::defaultManager().shutdown();
	Poco::ThreadPool::defaultPool().joinAll();
}


void TCPBundleActivatorTest::testListenerThatURIsCannotCarryIsRejected()
{
	const std::string listener("C:\\osp\\remoting.sock"s);
	_pFixture->pSettings->setString("remoting.tcp.listener"s, listener);

	try
	{
		_pFixture->start();
		fail("a Windows path is not a listener endpoint");
	}
	catch (Poco::InvalidArgumentException& exc)
	{
		assertTrue (exc.message().find(listener) != std::string::npos);
	}
}


void TCPBundleActivatorTest::testListenerWithServiceNameAsPortIsRejected()
{
	const std::string listener("127.0.0.1:http"s);
	_pFixture->pSettings->setString("remoting.tcp.listener"s, listener);

	try
	{
		_pFixture->start();
		fail("the URIs of the exported services cannot carry a service name as the port");
	}
	catch (Poco::InvalidArgumentException& exc)
	{
		assertTrue (exc.message().find(listener) != std::string::npos);
	}
}


void TCPBundleActivatorTest::testServiceRegisteredAfterStartIsExported()
{
	_pFixture->start();
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new StubRemoteService(EXPORTED_TYPE), properties);

	assertTrue (isExported(pRef));
	_pFixture->bundles.registry().unregisterService(pRef);
	assertTrue (!isExported(pRef));
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceIsExportedAgainAfterRestart()
{
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new StubRemoteService(EXPORTED_TYPE), properties);

	_pFixture->start();
	assertTrue (isExported(pRef));
	_pFixture->stop();
	assertTrue (!isExported(pRef));
	_pFixture->start();

	assertTrue (isExported(pRef));
	_pFixture->stop();
	_pFixture->bundles.registry().unregisterService(pRef);
}


void TCPBundleActivatorTest::testServiceUnregisteredDuringRegistrationIsNotExported()
{
	Unregisterer unregisterer(_pFixture->bundles.registry());
	_pFixture->start();
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new StubRemoteService(EXPORTED_TYPE), properties);

	assertTrue (_pFixture->bundles.registry().findByName("osp.test.remote"s) == nullptr);
	assertTrue (!isExported(pRef));
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceRegisteredAgainDuringUnregistrationIsExported()
{
	Reregisterer reregisterer(_pFixture->bundles.registry());
	_pFixture->start();
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new StubRemoteService(EXPORTED_TYPE), properties);

	_pFixture->bundles.registry().unregisterService(pRef);

	assertTrue (!isExported(pRef));
	Poco::OSP::ServiceRef::Ptr pAgain = reregisterer.service();
	assertTrue (pAgain != nullptr);
	assertTrue (isExported(pAgain));
	_pFixture->bundles.registry().unregisterService(pAgain);
	assertTrue (!isExported(pAgain));
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceWithSameObjectIdRegisteredAgainIsExported()
{
	Reregisterer reregisterer(_pFixture->bundles.registry());
	_pFixture->start();
	LogCapture capture(_pFixture->bundles.context()->logger().name());
	Poco::OSP::ServiceRef::Ptr pRef = registerRemoteService(_pFixture->bundles.registry(), "osp.test.remote"s);

	_pFixture->bundles.registry().unregisterService(pRef);

	assertTrue (!isExported(pRef));
	Poco::OSP::ServiceRef::Ptr pAgain = reregisterer.service();
	assertTrue (pAgain != nullptr);
	assertTrue (isExported(pAgain));
	// Exported once, removed once, exported again.
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(3);
	for (const auto& msg: messages)
	{
		assertTrue (msg.getPriority() > Poco::Message::PRIO_ERROR);
	}
	_pFixture->bundles.registry().unregisterService(pAgain);
	assertTrue (!isExported(pAgain));
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceRegisteredAgainConcurrentlyIsExported()
{
	ConcurrentReregisterer reregisterer(_pFixture->bundles.registry(), _pFixture->bundles.context()->logger());
	_pFixture->start();
	Poco::OSP::ServiceRef::Ptr pRef = registerRemoteService(_pFixture->bundles.registry(), "osp.test.remote"s);

	_pFixture->bundles.registry().unregisterService(pRef);
	reregisterer.resume();

	Poco::OSP::ServiceRef::Ptr pAgain = reregisterer.service();
	assertTrue (pAgain != nullptr);
	assertTrue (isExported(pAgain));
	_pFixture->bundles.registry().unregisterService(pAgain);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceUnregisteredDuringExportIsRemoved()
{
	Poco::AutoPtr<SwappingFactory> pFactory = new SwappingFactory(_pFixture->bundles.registry());
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, pFactory, properties);
	pFactory->setService(pRef);

	_pFixture->start();

	assertTrue (!pRef->properties().has("remoting.tcp.uri"s));
	Poco::OSP::ServiceRef::Ptr pOther = pFactory->other();
	assertTrue (pOther != nullptr);
	assertTrue (isExported(pOther));
	_pFixture->bundles.registry().unregisterService(pOther);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceRegisteredWhileObjectIsRemovedIsExported()
{
	RegisteringChannel::Ptr pChannel = new RegisteringChannel(_pFixture->bundles.registry());
	_pFixture->start();
	Poco::OSP::ServiceRef::Ptr pRef = registerRemoteService(_pFixture->bundles.registry(), "osp.test.remote"s);

	{
		const ChannelGuard guard(_pFixture->bundles.context()->logger(), pChannel);
		_pFixture->bundles.registry().unregisterService(pRef);
	}

	Poco::OSP::ServiceRef::Ptr pAgain = pChannel->service();
	assertTrue (pAgain != nullptr);
	assertTrue (isExported(pAgain));
	assertEqual (0, pChannel->errors());
	_pFixture->bundles.registry().unregisterService(pAgain);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testStandardExceptionOfServiceIsLogged()
{
	_pFixture->start();
	LogCapture capture(_pFixture->bundles.context()->logger().name());
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, new ThrowingFactory, properties);

	assertTrue (!pRef->properties().has("remoting.tcp.uri"s));
	_pFixture->bundles.registry().unregisterService(pRef);
	Poco::OSP::ServiceRef::Ptr pAgain = registerRemoteService(_pFixture->bundles.registry(), "osp.test.remote"s);
	assertTrue (isExported(pAgain));
	// The failed export, and the export of the service registered again.
	int errors = 0;
	for (const auto& msg: capture.channel().waitForMessages(2))
	{
		if (msg.getPriority() <= Poco::Message::PRIO_ERROR)
		{
			++errors;
			assertTrue (msg.getText().find("Remoting TCP transport: no service"s) != std::string::npos);
		}
	}
	assertEqual (1, errors);
	_pFixture->bundles.registry().unregisterService(pAgain);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceFactoryCanRegisterService()
{
	_pFixture->start();
	Poco::AutoPtr<RegisteringFactory> pFactory = new RegisteringFactory(_pFixture->bundles.registry());
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);

	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.factory"s, pFactory, properties);

	Poco::OSP::ServiceRef::Ptr pCreated = pFactory->created();
	assertTrue (pCreated != nullptr);
	assertTrue (isExported(pCreated));
	_pFixture->bundles.registry().unregisterService(pCreated);
	_pFixture->bundles.registry().unregisterService(pRef);
	_pFixture->stop();
}


void TCPBundleActivatorTest::testServiceRegisteredDuringStartIsExported()
{
	Poco::AutoPtr<RegisteringRemoteService> pService = new RegisteringRemoteService(_pFixture->bundles.registry());
	Poco::OSP::Properties properties;
	properties.set("remoting.tcp.enable"s, true);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.test.remote"s, pService, properties);

	_pFixture->start();

	Poco::OSP::ServiceRef::Ptr pRegistered = pService->registered();
	assertTrue (pRegistered != nullptr);
	assertTrue (isExported(pRegistered));
	_pFixture->stop();
	_pFixture->bundles.registry().unregisterService(pRegistered);
	_pFixture->bundles.registry().unregisterService(pRef);
}


void TCPBundleActivatorTest::setUp()
{
	_pFixture = std::make_unique<Fixture>();
}


void TCPBundleActivatorTest::tearDown()
{
	if (_pFixture->started) _pFixture->stop();
	_pFixture.reset();
}


CppUnit::Test* TCPBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TCPBundleActivatorTest");

#if defined(POCO_OS_FAMILY_UNIX)
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testRegularFileAtSocketPathIsKept);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testSocketFileHasConfiguredMode);
#endif
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testExportErrorDoesNotFailRegistration);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testListenerOnPortZeroExportsChosenPort);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testListenerThatURIsCannotCarryIsRejected);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testListenerWithServiceNameAsPortIsRejected);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceRegisteredAfterStartIsExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceIsExportedAgainAfterRestart);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceUnregisteredDuringRegistrationIsNotExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceRegisteredAgainDuringUnregistrationIsExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceWithSameObjectIdRegisteredAgainIsExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceRegisteredAgainConcurrentlyIsExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceUnregisteredDuringExportIsRemoved);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceRegisteredWhileObjectIsRemovedIsExported);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testStandardExceptionOfServiceIsLogged);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceFactoryCanRegisterService);
	CppUnit_addTest(pSuite, TCPBundleActivatorTest, testServiceRegisteredDuringStartIsExported);

	return pSuite;
}
