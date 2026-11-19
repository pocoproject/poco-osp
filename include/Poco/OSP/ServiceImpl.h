//
// ServiceImpl.h
//
// Library: OSP
// Package: Service
// Module:  ServiceImpl
//
// Definition of the ServiceImpl CRTP template class.
//
// Copyright (c) 2021-2025, Aleph ONE Software Engineering LLC.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_ServiceImpl_INCLUDED
#define OSP_ServiceImpl_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/AutoPtr.h"
#include "Poco/Logger.h"
#include "Poco/RWLock.h"
#include "Poco/Types.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/Util/Application.h"


namespace Poco::OSP {


template <typename Derived>
class ServiceImpl: public Service
	/// ServiceImpl is a CRTP base class for OSP services that provides
	/// common functionality.
	///
	/// ## Overview
	///
	/// This template class eliminates boilerplate code for:
	///   - Service type identification (type(), isA())
	///   - Service name (configurable, defaults to demangled class name)
	///   - Configuration access (merged from multiple sources)
	///   - Logger access
	///   - BundleContext management
	///
	/// ## Configuration
	///
	/// ServiceImpl provides a single merged configuration via config().
	/// The configuration is populated in order, with later entries
	/// overwriting earlier ones:
	///
	///   1. **Process configuration**: Global application configuration
	///      from PreferencesService (OSP) or Application::config() (standalone).
	///
	///   2. **Bundle configuration**: Bundle-specific properties from
	///      bundle.properties (only when running within a bundle).
	///
	///   3. **Service configuration**: Service-specific configuration
	///      passed to the constructor or added via setConfig().
	///
	/// ## Usage
	///
	/// Derived classes inherit from ServiceImpl<DerivedClass> and call
	/// the protected constructor:
	///
	/// Example:
	///
	///     class MyService: public Poco::OSP::ServiceImpl<MyService>
	///     {
	///     public:
	///         MyService(const std::string& name = SERVICE_NAME, BundleContext::Ptr pContext = nullptr):
	///             ServiceImpl(name, pContext)
	///         {
	///         }
	///     };
	///
	/// ## Automatic Features
	///
	/// The following are automatically provided:
	///   - SERVICE_NAME - static name for ServiceFinder (unique per derived type)
	///   - type() - returns typeid(Derived)
	///   - isA() - checks type compatibility
	///   - name() - returns the runtime service name
	///
	/// The following protected helpers are available:
	///   - context() - returns the BundleContext (nullptr if not in bundle)
	///   - config() - returns the merged configuration
	///   - setConfig() - adds entries to the configuration
	///   - logger() - returns a logger named after the service name
{
public:
	ServiceImpl(const ServiceImpl&) = delete;
	ServiceImpl(ServiceImpl&&) = delete;
	ServiceImpl& operator=(const ServiceImpl&) = delete;
	ServiceImpl& operator=(ServiceImpl&&) = delete;

	using Ptr = Poco::AutoPtr<Derived>;
	using ConfigPtr = Poco::AutoPtr<Poco::Util::AbstractConfiguration>;

	static inline const std::string SERVICE_NAME = Poco::demangleDot<Derived>();
		/// Default service name for compile-time lookup.
		///
		/// Used by ServiceFinder::find<Svc>() to look up services without
		/// an instance. The default (demangled class name) is automatically
		/// unique per derived type, which is required for ServiceFinder to
		/// correctly identify each service type.
		///
		/// ServiceRegistry allows only one service per name (throws
		/// ExistsException on duplicate). Instances of the same service
		/// type that are registered at the same time must be given
		/// different names.
		///
		/// Note: The instance method name() returns the actual runtime
		/// service name, which may differ from SERVICE_NAME if the service
		/// was registered with a custom name (e.g., from configuration).

	[[nodiscard]] const std::type_info& type() const override
		/// Returns the type information for the derived service class.
	{
		return typeid(Derived);
	}

	[[nodiscard]] bool isA(const std::type_info& otherType) const override
		/// Returns true if the service is of the given type
		/// or a subclass thereof.
	{
		std::string typeName(typeid(Derived).name());
		return typeName == otherType.name() || Service::isA(otherType);
	}

	[[nodiscard]] const std::string& name() const
		/// Returns the service name.
	{
		return _name;
	}

protected:

	ServiceImpl(const std::string& name = SERVICE_NAME,
		BundleContext::Ptr pContext = nullptr,
		ConfigPtr pServiceConf = nullptr): _pContext(pContext),
		_pConfig(new Poco::Util::MapConfiguration),
		_name(name),
		_logger(Poco::Logger::get(_name))
		/// Creates a ServiceImpl.
		///
		/// pContext is the BundleContext, or nullptr if the service
		/// is not running within a bundle.
		///
		/// pServiceConf, if provided, will be merged into the configuration
		/// after process and bundle configurations.
		///
		/// name is the service name, defaults to the demangled class name.
		///
		/// Configuration population order (later overwrites earlier):
		///   1. Process-wide configuration
		///   2. Bundle configuration
		///   3. Service configuration (pServiceConf)
	{
		// 1. Process-wide configuration (lowest priority)
		if (_pContext != nullptr)
		{
			ServiceRef::Ptr pPrefsRef =
				_pContext->registry().findByName(PreferencesService::SERVICE_NAME);
			if (pPrefsRef != nullptr)
			{
				PreferencesService::Ptr pPrefs = pPrefsRef->castedInstance<PreferencesService>();
				if (pPrefs != nullptr && pPrefs->configuration() != nullptr)
					copyConfig(pPrefs->configuration());
			}
		}
		else if (Poco::Util::Application::exists())
		{
			copyConfig(&Poco::Util::Application::instance().config());
		}

		// 2. Bundle configuration (overwrites process)
		if (_pContext != nullptr && _pContext->thisBundle() != nullptr)
		{
			copyConfig(&_pContext->thisBundle()->properties());
		}

		// 3. Service configuration (overwrites bundle)
		if (pServiceConf != nullptr)
		{
			copyConfig(pServiceConf);
		}
	}

	~ServiceImpl() override = default;
		/// Destroys the ServiceImpl.

	[[nodiscard]] BundleContext::Ptr context() const
		/// Returns the BundleContext, or nullptr if not running in a bundle.
	{
		Poco::RWLock::ScopedReadLock lock(_rwLock);
		return _pContext;
	}

	[[nodiscard]] ConfigPtr config() const
		/// Returns the merged configuration.
		///
		/// The configuration contains entries from process, bundle,
		/// and service configurations, with later sources overwriting
		/// earlier ones.
	{
		Poco::RWLock::ScopedReadLock lock(_rwLock);
		return _pConfig;
	}

	[[nodiscard]] ConfigPtr configSnapshot() const
		/// Returns a deep copy of the merged configuration.
		///
		/// The returned MapConfiguration is an independent copy that
		/// is safe to read without locks, even if setConfig() is called
		/// concurrently on the original.
	{
		Poco::RWLock::ScopedReadLock lock(_rwLock);
		Poco::AutoPtr<Poco::Util::MapConfiguration> pCopy = new Poco::Util::MapConfiguration;
		copyConfigInto(*_pConfig, *pCopy, "");
		return pCopy;
	}

	void setConfig(ConfigPtr pConf, ConfigPtr pRemoveConf = nullptr)
		/// Updates configuration entries.
		///
		/// If pRemoveConf is provided, those entries are removed first.
		/// Then entries from pConf are added, overwriting any existing
		/// entries with the same keys.
	{
		Poco::RWLock::ScopedWriteLock lock(_rwLock);
		if (pRemoveConf != nullptr)
			removeConfig(pRemoveConf);
		if (pConf != nullptr)
			copyConfig(pConf);
	}

	[[nodiscard]] Poco::Logger& logger() const
		/// Returns a logger for this service.
		///
		/// The logger is named after the service name.
	{
		return _logger;
	}

private:
	static void copyConfigInto(const Poco::Util::AbstractConfiguration& src,
		Poco::Util::MapConfiguration& dst, const std::string& root)
		/// Recursively copies all entries below root from src into dst.
	{
		std::vector<std::string> keys;
		src.keys(root, keys);
		for (const auto& key : keys)
		{
			std::string fullKey = root.empty() ? key : root + "." + key;
			if (src.hasProperty(fullKey))
			{
				dst.setString(fullKey, src.getRawString(fullKey));
			}
			copyConfigInto(src, dst, fullKey);
		}
	}

	void copyConfig(const Poco::Util::AbstractConfiguration* pSrc)
		/// Copies all entries from pSrc into _pConfig.
	{
		copyConfigInto(*pSrc, *_pConfig, "");
	}

	void removeConfig(const Poco::Util::AbstractConfiguration* pSrc)
		/// Removes all entries in pSrc from _pConfig.
	{
		removeConfigRecursive(pSrc, "");
	}

	void removeConfigRecursive(const Poco::Util::AbstractConfiguration* pSrc, const std::string& root)
		/// Recursively removes configuration entries.
	{
		std::vector<std::string> keys;
		pSrc->keys(root, keys);
		for (const auto& key : keys)
		{
			std::string fullKey = root.empty() ? key : root + "." + key;
			if (pSrc->hasProperty(fullKey))
			{
				_pConfig->remove(fullKey);
			}
			removeConfigRecursive(pSrc, fullKey);
		}
	}

	mutable Poco::RWLock _rwLock;
		/// Protects configuration members.

	BundleContext::Ptr _pContext;
		/// The bundle context, or nullptr if not running in a bundle.

	Poco::AutoPtr<Poco::Util::MapConfiguration> _pConfig;
		/// Merged configuration from process, bundle, and service sources.

	std::string _name;
		/// The service name.

	Poco::Logger& _logger;
		/// Cached logger reference.
};


} // namespace Poco::OSP


#endif // OSP_ServiceImpl_INCLUDED
