//
// BundleLoader.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleLoader
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/Timestamp.h"
#include "Poco/Environment.h"
#include "Poco/Exception.h"
#include "Poco/ScopedLock.h"
#include "Poco/Ascii.h"
#if defined(POCO_REQUIRE_LICENSE)
#include "Poco/Licensing/License.h"
#endif
#include <memory>
#include <algorithm>
#include <exception>
#include <set>
#include <string>
#include <vector>


using namespace std::string_literals;


using Poco::File;
using Poco::Path;
using Poco::Timestamp;
using Poco::Environment;
using Poco::Logger;


namespace Poco::OSP {


namespace
{
	thread_local std::string startingBundleName;
		/// The bundle whose start is in progress in this thread,
		/// see BundleLoader::StartingBundle.

	thread_local std::set<const Bundle*> resolvingBundles;
		/// The bundles whose resolution is in progress in this thread,
		/// see BundleLoader::resolveRequiredBundles().

	[[noreturn]] void throwCircularReference(const Bundle& bundle, const Bundle& required)
		/// Throws the BundleResolveException for a bundle that requires,
		/// directly or not, a bundle that requires it.
	{
		std::string msg("Circular reference detected involving ");
		msg += bundle.symbolicName();
		msg += " and ";
		msg += required.symbolicName();
		throw BundleResolveException(msg);
	}
}


BundleLoader::BundleLoader(CodeCache& codeCache, BundleFactory::Ptr pBundleFactory, BundleContextFactory::Ptr pBundleContextFactory, const std::string& osName, const std::string& osArch, bool autoUpdateCodeCache):
	_nextBundleId(1),
	_codeCache(codeCache),
	_autoUpdateCodeCache(autoUpdateCodeCache),
	_pBundleFactory(pBundleFactory),
	_pBundleContextFactory(pBundleContextFactory),
	_osName(osName),
	_osArch(osArch),
	_logger(Logger::get("osp.core.BundleLoader"s))
{
	makeValidFileName(_osName);
	makeValidFileName(_osArch);
}


BundleLoader::BundleLoader(CodeCache& codeCache, BundleFactory::Ptr pBundleFactory, BundleContextFactory::Ptr pBundleContextFactory, bool autoUpdateCodeCache):
	_nextBundleId(1),
	_codeCache(codeCache),
	_autoUpdateCodeCache(autoUpdateCodeCache),
	_pBundleFactory(pBundleFactory),
	_pBundleContextFactory(pBundleContextFactory),
#ifdef POCO_TARGET_OSNAME
	_osName(POCO_TARGET_OSNAME),
#else
	_osName(Environment::osName()),
#endif
#ifdef POCO_TARGET_OSARCH
	_osArch(POCO_TARGET_OSARCH),
#else
	_osArch(Environment::osArchitecture()),
#endif
	_logger(Logger::get("osp.core.BundleLoader"s))
{
	makeValidFileName(_osName);
#if (POCO_OS == POCO_OS_MAC_OS_X) && (POCO_ARCH == POCO_ARCH_AMD64)
	// Environment::osArchitecture() may return "i386" since the
	// kernel may be 32-bit while the userland is 64-bit
	_osArch = "x86_64";
#else
	makeValidFileName(_osArch);
#endif

	_logger.debug("os='%s' arch='%s'"s, _osName, _osArch);
}


BundleLoader::~BundleLoader()
{
#if defined(POCO_OSP_STATIC)
	for (BundleActivatorFactoryMap::iterator it = _bundleActivatorFactories.begin(); it != _bundleActivatorFactories.end(); ++it)
	{
		delete it->second;
	}
#endif
}


Bundle::ConstPtr BundleLoader::findBundle(const std::string& symbolicName) const
{
	// Internal locking: startBundle/stopBundle now release _mutex around
	// activator + event-firing calls, so findBundle can be called by
	// startDependencies without an outer lock. Poco::Mutex is recursive,
	// so callers that still hold _mutex (e.g. the snapshot phase of
	// startBundle) are unaffected.
	Poco::Mutex::ScopedLock lock(_mutex);
	BundleMap::const_iterator it = _bundles.find(symbolicName);
	if (it != _bundles.end())
		return it->second.pBundle;
	else
		return Bundle::Ptr();
}


Bundle::ConstPtr BundleLoader::findBundle(int id) const
{
	Poco::Mutex::ScopedLock lock(_mutex);
	BundleIdMap::const_iterator it = _bundleIds.find(id);
	if (it != _bundleIds.end())
		return it->second;
	else
		return Bundle::Ptr();
}


Bundle::Ptr BundleLoader::createBundle(const std::string& path)
{
	return _pBundleFactory->createBundle(*this, path);
}


Bundle::Ptr BundleLoader::loadBundle(const std::string& path)
{
	Bundle::Ptr pBundle(createBundle(path));
	loadBundle(pBundle);
	return pBundle;
}


void BundleLoader::loadBundle(Bundle::Ptr pBundle)
{
#if defined(POCO_REQUIRE_LICENSE)
	poco_verify_license(OSP);
#endif

	Poco::ScopedLockWithUnlock<Poco::Mutex> lock(_mutex);

	const std::string& symbolicName = pBundle->symbolicName();
	BundleMap::const_iterator it = _bundles.find(symbolicName);
	if (it == _bundles.end())
	{
		BundleInfo info;
		info.pBundle  = pBundle;
		info.pContext = _pBundleContextFactory->createBundleContext(*this, pBundle, _events);
		_bundles[symbolicName] = info;
		_bundleIds[pBundle->id()] = pBundle;
		lock.unlock();

		// Events are fired without _mutex: a delegate may call back into the loader.
		BundleEvent loadedEvent(pBundle, BundleEvent::EV_BUNDLE_LOADED);
		_events.bundleLoaded(this, loadedEvent);
	}
	else
	{
		std::string msg(symbolicName);
		msg += " version ";
		msg += pBundle->version().toString();
		msg += " from '";
		msg += pBundle->path();
		msg += "' conflicts with already loaded version ";
		msg += it->second.pBundle->version().toString();
		msg += " from '";
		msg += it->second.pBundle->path();
		msg += "'";
		throw BundleVersionConflictException(msg);
	}
}


void BundleLoader::unloadBundle(Bundle::Ptr pBundle)
{
	Poco::Mutex::ScopedLock lifecycleLock(pBundle->_lifecycleMutex);

	if (pBundle->isStarted())
		throw BundleStateException("An active bundle cannot be unloaded", pBundle->symbolicName());

	if (pBundle->isExtensionBundle())
	{
		Bundle::Ptr pExtendedBundle = pBundle->extendedBundle();
		if (pExtendedBundle != nullptr)
		{
			pExtendedBundle->removeExtensionBundle(pBundle);
		}
	}

	// The activator is unloaded, and the event fired, without _mutex: both
	// call into bundle code, which may call back into the loader.
	BundleInfo info;
	{
		Poco::Mutex::ScopedLock lock(_mutex);

		BundleMap::iterator it = _bundles.find(pBundle->symbolicName());
		if (it != _bundles.end())
		{
			info = it->second;
			_bundles.erase(it);
		}
		_bundleIds.erase(pBundle->id());
	}
	if (!info.pBundle.isNull())
	{
		unloadActivator(info);
	}
	BundleEvent unloadedEvent(pBundle, BundleEvent::EV_BUNDLE_UNLOADED);
	_events.bundleUnloaded(this, unloadedEvent);
}


void BundleLoader::listBundles(std::vector<Bundle::Ptr>& bundles) const
{
	Poco::Mutex::ScopedLock lock(_mutex);

	bundles.clear();
	bundles.reserve(_bundles.size());
	for (BundleMap::const_iterator it = _bundles.begin(); it != _bundles.end(); ++it)
	{
		bundles.push_back(it->second.pBundle);
	}
}


void BundleLoader::listBundles(std::vector<Bundle::Ptr>& bundles, BundleFilter::Ptr pFilter) const
{
	Poco::Mutex::ScopedLock lock(_mutex);

	bundles.clear();
	bundles.reserve(_bundles.size());
	for (BundleMap::const_iterator it = _bundles.begin(); it != _bundles.end(); ++it)
	{
		if (pFilter->accept(it->second.pBundle))
		{
			bundles.push_back(it->second.pBundle);
		}
	}
}


void BundleLoader::listRequiringBundles(Bundle::ConstPtr pBundle, std::vector<Bundle::Ptr>& bundles) const
{
	std::vector<Bundle::Ptr> sorted;
	sortBundles(sorted);

	// In dependency order, a bundle comes after the bundles it requires.
	bundles.clear();
	std::set<std::string> required;
	required.insert(pBundle->symbolicName());
	for (std::vector<Bundle::Ptr>::const_reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it)
	{
		if (!(*it)->isResolved()) continue;
		const Bundle::ResolvedDependencies deps = (*it)->resolvedDependencies();
		const bool requiresBundle = std::any_of(deps.begin(), deps.end(), [&required](const Bundle::ResolvedDependency& dep)
		{
			return required.contains(dep.symbolicName);
		});
		if (requiresBundle)
		{
			required.insert((*it)->symbolicName());
			bundles.push_back(*it);
		}
	}
	std::reverse(bundles.begin(), bundles.end());
}


void BundleLoader::resolveAllBundles()
{
	// Resolved without _mutex, so that the resolve events are fired without it.
	std::vector<Bundle::Ptr> bundles;
	listBundles(bundles);

	for (std::vector<Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if (!(*it)->isResolved())
		{
			try
			{
				(*it)->resolve();
			}
			catch (Poco::Exception& exc)
			{
				// Another thread has resolved the bundle in the meantime.
				if ((*it)->isResolved()) continue;

				std::string msg("Failed to resolve bundle ");
				msg += (*it)->symbolicName();
				msg += ": ";
				msg += exc.displayText();
				_logger.error(msg);

				BundleError error;
				error.pBundle = *it;
				error.targetState = Bundle::BUNDLE_RESOLVED;
				error.pException = &exc;
				bundleError(this, error);
			}
		}
	}
}


namespace
{
	struct RunLevelLess
	{
		bool operator () (const Bundle::Ptr& p1, const Bundle::Ptr& p2) const
		{
			return p1->runLevel() < p2->runLevel();
		}
	};
}


void BundleLoader::startAllBundles()
{
	std::vector<Bundle::Ptr> bundles;
	listBundles(bundles);
	std::sort(bundles.begin(), bundles.end(), RunLevelLess());

	for (std::vector<Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if ((*it)->state() == Bundle::BUNDLE_RESOLVED && !(*it)->lazyStart())
		{
			startingBundleName.clear();
			try
			{
				(*it)->start();
			}
			catch (Poco::Exception& exc)
			{
				handleStartError(*it, exc.displayText(), exc);
			}
			catch (std::exception& exc)
			{
				Poco::SystemException se(exc.what());
				handleStartError(*it, exc.what(), se);
			}
			catch (...)
			{
				Poco::UnhandledException ue("unknown exception");
				handleStartError(*it, ue.displayText(), ue);
			}
		}
	}
}


void BundleLoader::handleStartError(Bundle::Ptr pBundle, const std::string& errorMsg, Poco::Exception& exc)
{
	const std::string failedBundle(startingBundleName.empty() ? pBundle->symbolicName() : startingBundleName);
	std::string msg("Failed to start bundle ");
	if (failedBundle != pBundle->symbolicName())
	{
		msg += failedBundle;
		msg += " required by ";
		msg += pBundle->symbolicName();
		msg += ": ";
	}
	else
	{
		msg += pBundle->symbolicName();
		msg += ": ";
	}
	msg += errorMsg;
	_logger.error(msg);

	BundleError error;
	error.pBundle = pBundle;
	error.targetState = Bundle::BUNDLE_STARTING;
	error.pException = &exc;
	bundleError(this, error);
}


void BundleLoader::stopAllBundles()
{
	std::vector<Bundle::Ptr> bundles;
	sortBundles(bundles);
	for (std::vector<Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if ((*it)->isActive())
		{
			stopBundleOnShutdown(*it);
		}
	}
}


void BundleLoader::stopBundleOnShutdown(Bundle::Ptr pBundle)
{
	try
	{
		pBundle->stop();
	}
	catch (Poco::Exception& exc)
	{
		handleStopError(pBundle, exc);
	}
	catch (std::exception& exc)
	{
		Poco::SystemException se(exc.what());
		handleStopError(pBundle, se);
	}
	catch (...)
	{
		Poco::UnhandledException ue("unknown exception");
		handleStopError(pBundle, ue);
	}
}


void BundleLoader::handleStopError(Bundle::Ptr pBundle, Poco::Exception& exc)
{
	_logger.error("Failed to stop bundle %s: %s"s, pBundle->symbolicName(), exc.displayText());

	BundleError error;
	error.pBundle = pBundle;
	error.targetState = Bundle::BUNDLE_RESOLVED;
	error.pException = &exc;
	bundleError(this, error);
}


void BundleLoader::unloadAllBundles(bool unloadLibraries)
{
	std::vector<Bundle::Ptr> bundles;
	sortBundles(bundles);
	for (std::vector<Bundle::Ptr>::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		if ((*it)->isActive())
		{
			stopBundleOnShutdown(*it);
		}
		// A bundle that cannot be stopped is still active and stays loaded.
		if ((*it)->isActive())
		{
			continue;
		}
		if (unloadLibraries)
		{
			unloadBundle(*it);
		}
		else
		{
			// Clean up data structures without calling SharedLibrary::unload().
			// The OS reclaims DLLs at process exit.
			//
			// Extract pClassLoader into a local before releasing _mutex so
			// the AutoPtr's ref-drop (which destroys the ClassLoader, its
			// MetaObjects, BundleActivators, ExtensionPoints, and triggers
			// AbstractEvent operator-= chains via those destructors) runs
			// AFTER _mutex is released. Holding _mutex across that chain
			// previously created a BundleLoader::_mutex -> Delegate::_mutex
			// lock-order edge that closed a cycle with the inverse edge
			// observed during startup (BundleEvent.notify -> contextForBundle).
			ActivatorClassLoaderPtr pToRelease;
			{
				Poco::Mutex::ScopedLock lock(_mutex);
				BundleMap::iterator itMap = _bundles.find((*it)->symbolicName());
				if (itMap != _bundles.end())
				{
					pToRelease = itMap->second.pClassLoader;
					itMap->second.pClassLoader = nullptr;
					if (!pToRelease.isNull()) itMap->second.pBundle->setActivator(nullptr);
					_bundles.erase(itMap);
				}
				_bundleIds.erase((*it)->id());
			}
			// pToRelease's destructor runs here, with _mutex released.
		}
	}
}


void BundleLoader::resolveBundle(Bundle* pBundle)
{
	resolvingBundles.insert(pBundle);
	try
	{
		for (;;)
		{
			std::vector<Bundle::Ptr> unresolved;
			{
				Poco::Mutex::ScopedLock lock(_mutex);

				unresolvedRequirements(pBundle, unresolved);
				if (unresolved.empty())
				{
					resolveBundleImpl(pBundle);
					break;
				}
				checkCircularReference(pBundle, unresolved);
			}
			resolveRequiredBundles(pBundle, unresolved);
		}
	}
	catch (...)
	{
		resolvingBundles.erase(pBundle);
		throw;
	}
	resolvingBundles.erase(pBundle);
}


void BundleLoader::unresolvedRequirements(Bundle* pBundle, std::vector<Bundle::Ptr>& unresolved) const
{
	unresolvedDependencies(pBundle, unresolved);
	if (pBundle->isExtensionBundle())
	{
		Bundle::Ptr pExtendedBundle = pBundle->extendedBundle();
		if (pExtendedBundle != nullptr && !pExtendedBundle->isResolved())
		{
			unresolved.push_back(pExtendedBundle);
		}
	}
}


void BundleLoader::checkCircularReference(Bundle* pBundle, const std::vector<Bundle::Ptr>& required) const
{
	std::set<const Bundle*> visited;
	std::vector<Bundle::Ptr> pending(required);
	while (!pending.empty())
	{
		Bundle::Ptr pRequired = pending.back();
		pending.pop_back();
		if (!visited.insert(pRequired.get()).second) continue;

		std::vector<Bundle::Ptr> next;
		unresolvedRequirements(pRequired.get(), next);
		for (const Bundle::Ptr& pNext: next)
		{
			if (pNext.get() == pBundle) throwCircularReference(*pRequired, *pBundle);
			pending.push_back(pNext);
		}
	}
}


void BundleLoader::resolveRequiredBundles(Bundle* pBundle, const std::vector<Bundle::Ptr>& required)
{
	for (Bundle::Ptr pRequired: required)
	{
		if (resolvingBundles.contains(pRequired.get()))
		{
			throwCircularReference(*pBundle, *pRequired);
		}
		// Waits for a thread that is resolving the required bundle.
		Poco::Mutex::ScopedLock lock(pRequired->_lifecycleMutex);
		if (pRequired->state() != Bundle::BUNDLE_INSTALLED) continue;
		try
		{
			pRequired->resolve();
		}
		catch (Poco::Exception& exc)
		{
			if (_logger.debug())
			{
				_logger.debug("Cannot resolve bundle %s, required by %s: %s"s,
					pRequired->symbolicName(), pBundle->symbolicName(), exc.displayText());
			}
			throw;
		}
	}
}


void BundleLoader::resolveBundleImpl(Bundle* pBundle)
{
	if (_logger.debug())
	{
		_logger.debug("Resolving bundle %s."s, pBundle->symbolicName());
	}

	try
	{
		Bundle::ModuleProviders providers = resolveModules(pBundle);
		if (!providers.empty())
		{
			resolveProviders(pBundle, providers);
		}

		const BundleManifest::Dependencies& deps = pBundle->requiredBundles();
		if (!deps.empty())
		{
			resolveDependencies(pBundle, deps);
		}

		{
			Poco::FastMutex::ScopedLock resolutionLock(pBundle->_resolutionMutex);
			std::swap(providers, pBundle->_moduleProviders);
		}
	}
	catch (...)
	{
		pBundle->clearResolvedDependencies();
		throw;
	}

	if (_logger.debug())
	{
		_logger.debug("Installing libraries for %s."s, pBundle->symbolicName());
	}
	installLibraries(pBundle);

	if (pBundle->isExtensionBundle())
	{
		Bundle::Ptr pExtendedBundle = pBundle->extendedBundle();
		if (pExtendedBundle != nullptr)
		{
			pExtendedBundle->addExtensionBundle(pBundle);
			if (_logger.information())
			{
				_logger.information("Bundle %s successfully extended bundle %s."s, pBundle->symbolicName(), pExtendedBundle->symbolicName());
			}
		}
		else if (_logger.warning())
		{
			_logger.warning("Bundle %s cannot extend unknown bundle %s."s, pBundle->symbolicName(), pBundle->manifest().extendedBundle());
		}
	}

	if (_logger.information())
	{
		_logger.information("Bundle %s resolved."s, pBundle->symbolicName());
	}
}


void BundleLoader::resolveDependencies(Bundle* pBundle, const BundleManifest::Dependencies& deps, Bundle::ResolvedDependencies* pResolved)
{
	if (_logger.debug())
	{
		_logger.debug("Resolving dependencies for bundle %s"s, pBundle->symbolicName());
	}

	for (BundleManifest::Dependencies::const_iterator it = deps.begin(); it != deps.end(); ++it)
	{
		resolveDependency(pBundle, *it, pResolved);
	}
}


Bundle::ModuleProviders BundleLoader::resolveModules(Bundle* pBundle) const
{
	if (_logger.debug())
	{
		_logger.debug("Resolving modules for bundle %s."s, pBundle->symbolicName());
	}

	Bundle::ModuleProviders providers;
	const BundleManifest::Dependencies& moduleDeps = pBundle->requiredModules();
	for (BundleManifest::Dependencies::const_iterator itd = moduleDeps.begin(); itd != moduleDeps.end(); ++itd)
	{
		Bundle::ModuleProvider provider;
		provider.module = *itd;
		for (BundleMap::const_iterator itb = _bundles.begin(); itb != _bundles.end(); ++itb)
		{
			if (isProvidingModule(itb->second.pBundle, *itd))
			{
				Bundle::ResolvedDependency dep;
				dep.symbolicName = itb->first;
				dep.version = itb->second.pBundle->version();
				dep.id = itb->second.pBundle->id();
				provider.bundles.push_back(dep);
			}
		}
		if (provider.bundles.empty())
		{
			std::string msg("Module ");
			msg += itd->symbolicName;
			msg += ", version ";
			msg += itd->versions.toString();
			msg += " required by bundle ";
			msg += pBundle->symbolicName();
			msg += " not available";
			throw BundleResolveException(msg);
		}
		providers.push_back(provider);
	}
	return providers;
}


void BundleLoader::resolveProviders(Bundle* pBundle, const Bundle::ModuleProviders& providers, Bundle::ResolvedDependencies* pResolved)
{
	if (_logger.debug())
	{
		_logger.debug("Resolving module providers for bundle %s."s, pBundle->symbolicName());
	}

	for (Bundle::ModuleProviders::const_iterator itp = providers.begin(); itp != providers.end(); ++itp)
	{
		for (Bundle::ResolvedDependencies::const_iterator itb = itp->bundles.begin(); itb != itp->bundles.end(); ++itb)
		{
			BundleManifest::Dependency dep;
			dep.symbolicName = itb->symbolicName;
			dep.versions = itb->version;
			resolveDependency(pBundle, dep, pResolved);
		}
	}
}


void BundleLoader::startBundle(Bundle* pBundle)
{
	// Pin pBundle's lifetime for the duration. Caller normally holds a Ptr
	// already (Bundle::start passes `this`), but making it explicit keeps the
	// invariant local: pBundle (and the BundleActivator it owns) cannot be
	// destroyed while we are running, even though we release _mutex below.
	Bundle::Ptr keepAlive(pBundle, true);

	if (_logger.debug())
	{
		_logger.debug("Starting bundle %s."s, pBundle->symbolicName());
	}

	// Phase 1: validate bundle is known. No state held across the unlock.
	{
		Poco::Mutex::ScopedLock lock(_mutex);
		if (!_bundles.contains(pBundle->symbolicName()))
			throw BundleException("The bundle loader does not know the bundle", pBundle->symbolicName());
	}

	// Phase 1b: a required bundle may have been replaced since pBundle was
	// resolved. The replacement is resolved WITHOUT _mutex held, for the same
	// reason the dependencies are started without it below: Bundle::resolve()
	// fires bundleResolving/bundleResolved. pBundle is resolved against it
	// under _mutex only once that leaves nothing to fire.
	refreshResolvedDependencies(pBundle);

	// Phase 2: start dependencies WITHOUT _mutex held. Each dependency's
	// Bundle::start() fires bundleStarting/bundleStarted events; event
	// delegates can call back into contextForBundle (which now acquires
	// _mutex itself). Running this outside _mutex avoids a lock-order cycle
	// between _mutex and AbstractEvent's notification mutex.
	startDependencies(pBundle);

	// Phase 3: load activator and copy out everything we'll need next.
	// loadActivator mutates BundleInfo::pClassLoader / pBundle->setActivator
	// in place, so _mutex must be held for that call.
	BundleActivator* pActivator = nullptr;
	BundleContext::Ptr pContext;
	{
		Poco::Mutex::ScopedLock lock(_mutex);
		BundleMap::iterator it = _bundles.find(pBundle->symbolicName());
		if (it == _bundles.end())
		{
			// Bundle was uninstalled while we were starting its dependencies.
			throw BundleException("The bundle loader does not know the bundle", pBundle->symbolicName());
		}
		pActivator = loadActivator(it->second);
		pContext   = it->second.pContext;
	}

	// Phase 4: invoke the activator outside _mutex. pBundle (kept alive by
	// keepAlive) owns pActivator; pContext is refcounted by AutoPtr. Both
	// outlive any concurrent map mutation.
	if (pActivator != nullptr)
	{
		_logger.debug("Invoking BundleActivator::start()"s);
		pActivator->start(pContext);
	}
	if (_logger.information())
	{
		_logger.information("Bundle %s started."s, pBundle->symbolicName());
	}
}


BundleLoader::StartingBundle::StartingBundle(const Bundle& bundle):
	_previous(startingBundleName),
	_uncaughtExceptions(std::uncaught_exceptions())
{
	startingBundleName = bundle.symbolicName();
}


BundleLoader::StartingBundle::~StartingBundle()
{
	if (std::uncaught_exceptions() == _uncaughtExceptions)
	{
		startingBundleName.swap(_previous);
	}
}


void BundleLoader::stopBundle(Bundle* pBundle)
{
	// Symmetrical to startBundle: pin pBundle so pActivator survives
	// even if the map entry is mutated while _mutex is released.
	Bundle::Ptr keepAlive(pBundle, true);

	if (_logger.debug())
	{
		_logger.debug("Stopping bundle %s."s, pBundle->symbolicName());
	}

	// Look up + copy out under _mutex.
	BundleActivator* pActivator = nullptr;
	BundleContext::Ptr pContext;
	{
		Poco::Mutex::ScopedLock lock(_mutex);
		BundleMap::iterator it = _bundles.find(pBundle->symbolicName());
		if (it == _bundles.end())
			throw BundleException("The bundle loader does not know the bundle", pBundle->symbolicName());
		pActivator = pBundle->activator();
		pContext   = it->second.pContext;
	}

	// Invoke the activator outside _mutex so that bundleStopping/bundleStopped
	// events (and any delegate callbacks into contextForBundle) cannot form
	// a lock-order cycle.
	if (pActivator != nullptr)
	{
		_logger.debug("Invoking BundleActivator::stop()"s);
		pActivator->stop(pContext);
	}
	if (_logger.information())
	{
		_logger.information("Bundle %s stopped."s, pBundle->symbolicName());
	}
}


void BundleLoader::uninstallBundle(Bundle* pBundle)
{
	if (_logger.debug())
	{
		_logger.debug("Uninstalling bundle %s."s, pBundle->symbolicName());
	}

	if (pBundle->isExtensionBundle())
	{
		Bundle::Ptr pExtendedBundle = pBundle->extendedBundle();
		if (pExtendedBundle != nullptr)
		{
			pExtendedBundle->removeExtensionBundle(pBundle);
		}
	}

	// As in unloadBundle(), the activator is unloaded, and the event
	// fired, without _mutex.
	BundleInfo info;
	{
		Poco::Mutex::ScopedLock lock(_mutex);

		BundleMap::iterator it = _bundles.find(pBundle->symbolicName());
		if (it != _bundles.end())
		{
			info = it->second;
			_bundles.erase(it);
		}
		_bundleIds.erase(pBundle->id());
	}
	if (!info.pBundle.isNull())
	{
		unloadActivator(info);
	}
	uninstallLibraries(pBundle);

	if (info.deleteOnUninstall.has_value() && *info.deleteOnUninstall)
	{
		// The bundle has been removed from the loader, so the uninstall is
		// completed even if its file or directory cannot be deleted.
		try
		{
			File(pBundle->path()).remove(true);
		}
		catch (Poco::Exception& exc)
		{
			_logger.warning("Bundle %s is uninstalled, but its file or directory '%s' cannot be deleted, in full or "
				"in part, and may be loaded again at the next start: %s"s,
				pBundle->symbolicName(), pBundle->path(), exc.displayText());
		}
	}
	else if (!info.deleteOnUninstall.has_value() && _logger.information())
	{
		_logger.information("The file or directory '%s' of bundle %s is kept, because it is not in the primary bundle repository, where bundles are installed."s,
			pBundle->path(), pBundle->symbolicName());
	}

	BundleEvent unloadedEvent(pBundle, BundleEvent::EV_BUNDLE_UNLOADED);
	_events.bundleUnloaded(this, unloadedEvent);

	if (_logger.information())
	{
		_logger.information("Bundle %s uninstalled."s, pBundle->symbolicName());
	}
}


void BundleLoader::setDeleteOnUninstall(Bundle::ConstPtr pBundle, bool deleteOnUninstall)
{
	Poco::Mutex::ScopedLock lock(_mutex);

	BundleMap::iterator it = _bundles.find(pBundle->symbolicName());
	if (it != _bundles.end())
		it->second.deleteOnUninstall = deleteOnUninstall;
	else
		throw BundleException("The bundle loader does not know the bundle", pBundle->symbolicName());
}


BundleContext::Ptr BundleLoader::contextForBundle(Bundle::ConstPtr pBundle) const
{
	Poco::Mutex::ScopedLock lock(_mutex);

	BundleMap::const_iterator it = _bundles.find(pBundle->symbolicName());
	if (it != _bundles.end())
		return it->second.pContext;
	else
		throw BundleException("The bundle loader does not know the bundle", pBundle->symbolicName());
}


void BundleLoader::resolveDependency(Bundle* pBundle, const BundleManifest::Dependency& dependency, Bundle::ResolvedDependencies* pResolved)
{
	Bundle::Ptr pDepBundle(findBundle(dependency.symbolicName));
	if (pDepBundle != nullptr)
	{
		if (dependency.versions.isEmpty() || dependency.versions.isInRange(pDepBundle->version()))
		{
			// resolveBundle() and refreshResolvedDependencies() have
			// resolved the required bundles before.
			if (!pDepBundle->isResolved())
				throw BundleResolveException("Required bundle not resolved", pDepBundle->symbolicName());

			Bundle::ResolvedDependency resolvedDep;
			resolvedDep.symbolicName = pDepBundle->symbolicName();
			resolvedDep.version = pDepBundle->version();
			resolvedDep.id = pDepBundle->id();
			if (pResolved != nullptr)
			{
				bool known = false;
				for (Bundle::ResolvedDependencies::const_iterator it = pResolved->begin(); it != pResolved->end(); ++it)
				{
					if (it->symbolicName == resolvedDep.symbolicName)
					{
						known = true;
						break;
					}
				}
				if (!known) pResolved->push_back(resolvedDep);
			}
			else
			{
				pBundle->addResolvedDependency(resolvedDep);
			}
		}
		else
		{
			std::string msg(dependency.symbolicName);
			msg += ", requested: ";
			msg += dependency.versions.toString();
			msg += ", available: ";
			msg += pDepBundle->version().toString();
			throw BundleResolveException("Required bundle not available in requested version", msg);
		}
	}
	else throw BundleResolveException("Required bundle not available", dependency.symbolicName);
}


bool BundleLoader::hasStaleDependencies(const Bundle* pBundle) const
{
	const Bundle::ResolvedDependencies deps = pBundle->resolvedDependencies();
	for (Bundle::ResolvedDependencies::const_iterator it = deps.begin(); it != deps.end(); ++it)
	{
		// A replacement is a new Bundle with a new ID, whatever its version
		// and however far it has come (another dependent may have resolved
		// or started it already).
		Bundle::ConstPtr pDepBundle(findBundle(it->symbolicName));
		if (pDepBundle == nullptr || pDepBundle->id() != it->id) return true;
	}
	return false;
}


void BundleLoader::unresolvedDependencies(Bundle* pBundle, std::vector<Bundle::Ptr>& unresolved) const
{
	// A required bundle outside the version range fails the resolution and
	// must not be resolved on the way.
	const BundleManifest::Dependencies& deps = pBundle->requiredBundles();
	for (BundleManifest::Dependencies::const_iterator it = deps.begin(); it != deps.end(); ++it)
	{
		Bundle::Ptr pDepBundle(findBundle(it->symbolicName));
		if (pDepBundle != nullptr && !pDepBundle->isResolved() &&
			(it->versions.isEmpty() || it->versions.isInRange(pDepBundle->version())))
		{
			unresolved.push_back(pDepBundle);
		}
	}

	const Bundle::ModuleProviders providers = resolveModules(pBundle);
	for (Bundle::ModuleProviders::const_iterator itp = providers.begin(); itp != providers.end(); ++itp)
	{
		for (Bundle::ResolvedDependencies::const_iterator itb = itp->bundles.begin(); itb != itp->bundles.end(); ++itb)
		{
			Bundle::Ptr pDepBundle(findBundle(itb->symbolicName));
			if (pDepBundle != nullptr && !pDepBundle->isResolved()) unresolved.push_back(pDepBundle);
		}
	}
}


void BundleLoader::refreshResolvedDependencies(Bundle* pBundle)
{
	// Every pass either resolves what it has found or throws; another pass is
	// only needed if a required bundle has been replaced again in between.
	for (;;)
	{
		std::vector<Bundle::Ptr> unresolved;
		{
			Poco::Mutex::ScopedLock lock(_mutex);

			if (!hasStaleDependencies(pBundle)) return;

			unresolvedDependencies(pBundle, unresolved);
			if (unresolved.empty())
			{
				resolveAgain(pBundle);
				return;
			}
		}

		// Without _mutex. A failure is final: resolving the bundle again
		// under _mutex would only fail for the same reason.
		resolveRequiredBundles(pBundle, unresolved);
	}
}


void BundleLoader::resolveAgain(Bundle* pBundle)
{
	if (_logger.information())
	{
		_logger.information("Dependencies of bundle %s have changed - resolving again."s, pBundle->symbolicName());
	}

	// Resolved dependencies are pinned to the bundle they were resolved
	// against, and a RESOLVED bundle cannot be resolved again. Redo the
	// dependency part of resolveBundle() against the bundles available now;
	// the manifest's version ranges decide whether the replacement is
	// acceptable. The new resolution is built on the side and replaces the
	// previous one in one step: a failure leaves the bundle reporting what it
	// was resolved against, and a concurrent reader never sees a partial list.
	Bundle::ResolvedDependencies resolved;
	Bundle::ModuleProviders providers = resolveModules(pBundle);
	if (!providers.empty())
	{
		resolveProviders(pBundle, providers, &resolved);
	}

	const BundleManifest::Dependencies& deps = pBundle->requiredBundles();
	if (!deps.empty())
	{
		resolveDependencies(pBundle, deps, &resolved);
	}

	pBundle->setResolution(resolved, providers);
}


void BundleLoader::startDependencies(Bundle* pBundle)
{
	// resolvedDependencies() returns a copy; the loop below runs unlocked.
	const Bundle::ResolvedDependencies deps = pBundle->resolvedDependencies();
	for (Bundle::ResolvedDependencies::const_iterator it = deps.begin(); it != deps.end(); ++it)
	{
		Bundle::Ptr pDepBundle(findBundle(it->symbolicName));
		if (pDepBundle != nullptr && pDepBundle->version() == it->version)
		{
			// Waits for a thread that is starting the required bundle.
			Poco::Mutex::ScopedLock lock(pDepBundle->_lifecycleMutex);
			if (!pDepBundle->isActive())
			{
				pDepBundle->start();
			}
		}
		else throw BundleResolveException("A required bundle is no longer available", it->symbolicName);
	}
}


BundleActivator* BundleLoader::loadActivator(BundleInfo& bundleInfo)
{
	BundleActivator* pActivator(nullptr);
#if defined(POCO_OSP_STATIC)
	pActivator = bundleInfo.pBundle->activator();
	if (pActivator == nullptr)
	{
		const std::string& activatorClass = bundleInfo.pBundle->activatorClass();
		if (!activatorClass.empty())
		{
			BundleActivatorFactoryMap::const_iterator it = _bundleActivatorFactories.find(activatorClass);
			if (it == _bundleActivatorFactories.end()) throw Poco::NotFoundException("Unknown BundleActivator class", activatorClass);
			pActivator = it->second->createInstance();
			bundleInfo.pBundle->setActivator(pActivator);
		}
	}
#else
	if (bundleInfo.pClassLoader == nullptr)
	{
		// kept unless loading the activator fails, so that a start that
		// fails to load it fails again on the next start
		ActivatorClassLoaderPtr pClassLoader(new ActivatorClassLoader);
		const std::string& activatorClass = bundleInfo.pBundle->activatorClass();
		if (!activatorClass.empty())
		{
			std::string libPath(libraryPathFor(bundleInfo.pBundle));
			if (_logger.debug())
			{
				_logger.debug("Loading library %s."s, libPath);
			}
			pClassLoader->loadLibrary(libPath);
			try
			{
				const ActivatorClassLoader::Meta& meta = pClassLoader->classFor(activatorClass);
				if (meta.canCreate())
				{
					pActivator = meta.create();
					meta.autoDelete(pActivator);
				}
				else
				{
					pActivator = &meta.instance();
				}
				bundleInfo.pBundle->setActivator(pActivator);
			}
			catch (...)
			{
				// the class loader's destructor does not unload the library
				pClassLoader->unloadLibrary(libPath);
				throw;
			}
		}
		bundleInfo.pClassLoader = pClassLoader;
	}
	else
	{
		pActivator = bundleInfo.pBundle->activator();
	}
#endif
	return pActivator;
}


void BundleLoader::unloadActivator(BundleInfo& bundleInfo)
{
#if !defined(POCO_OSP_STATIC)
	if (bundleInfo.pClassLoader != nullptr)
	{
		// Reset before the library that holds the activator is unloaded.
		bundleInfo.pBundle->setActivator(nullptr);
		if (!bundleInfo.pBundle->activatorClass().empty())
		{
			std::string libPath(libraryPathFor(bundleInfo.pBundle));
			if (_logger.debug())
			{
				_logger.debug("Unloading library %s."s, libPath);
			}
			bundleInfo.pClassLoader->unloadLibrary(libPath);
		}
		bundleInfo.pClassLoader = nullptr;
	}
#endif
}


void BundleLoader::installLibraries(Bundle* pBundle)
{
	std::vector<std::string> libs;
	listLibraries(pBundle, libs);

	CodeCache::Lock ccLock(_codeCache);

	for (std::vector<std::string>::iterator it = libs.begin(); it != libs.end(); ++it)
	{
		const Path p(libraryDirectory(), *it);

		if (_codeCache.hasLibrary(*it))
		{
			if (_autoUpdateCodeCache)
			{
				Poco::Timestamp cachedFileTS(_codeCache.libraryTimestamp(*it));
				Poco::Timestamp bundledFileTS(pBundle->storage().lastModified(p.toString(Path::PATH_UNIX)));
				if (cachedFileTS < bundledFileTS)
				{
					installLibrary(pBundle, p, &bundledFileTS);
				}
				else
				{
					if (_logger.debug())
					{
						_logger.debug("Same or newer version of library found in cache: %s"s, *it);
					}
				}
			}
			else
			{
				if (_logger.debug())
				{
					_logger.debug("Library found in cache: %s"s, *it);
				}
			}
		}
		else
		{
			installLibrary(pBundle, p);
		}
	}
}


void BundleLoader::installLibrary(Bundle* pBundle, const Poco::Path& p, const Poco::Timestamp* pTS)
{
	if (_logger.debug())
	{
		_logger.debug("Installing library %s."s, p.toString(Path::PATH_UNIX));
	}
	std::unique_ptr<std::istream> pStream(pBundle->storage().getResource(p.toString(Path::PATH_UNIX)));
	if (pStream != nullptr)
	{
		_codeCache.installLibrary(p.getFileName(), *pStream);
		if (pTS != nullptr)
		{
			File f(_codeCache.pathFor(p.getFileName(), false));
			try
			{
				f.setLastModified(*pTS);
			}
			catch (Poco::Exception& exc)
			{
				_logger.warning("Failed to set timestamp on %s: %s"s, p.toString(), exc.displayText());
			}
		}
	}
}


void BundleLoader::uninstallLibraries(Bundle* pBundle)
{
	std::vector<std::string> libs;
	listLibraries(pBundle, libs);

	// Under _mutex: resolveBundleImpl() installs or relies on a library under
	// it, before the bundle's state is RESOLVED.
	Poco::Mutex::ScopedLock lock(_mutex);
	CodeCache::Lock ccLock(_codeCache);

	for (std::vector<std::string>::iterator it = libs.begin(); it != libs.end(); ++it)
	{
		// A bundle that has never been resolved has no library in the code cache.
		if (!_codeCache.hasLibrary(*it)) continue;

		const Path p(libraryDirectory(), *it);
		const bool shipped = std::any_of(_bundles.begin(), _bundles.end(),
			[pBundle, &p](BundleMap::value_type& entry)
		{
			Bundle::Ptr& pOther = entry.second.pBundle;
			return pOther.get() != pBundle
				&& std::unique_ptr<std::istream>(pOther->storage().getResource(p.toString(Path::PATH_UNIX))) != nullptr;
		});
		if (shipped)
		{
			if (_logger.debug())
			{
				_logger.debug("Library %s is kept for another bundle."s, *it);
			}
			continue;
		}

		if (_logger.debug())
		{
			_logger.debug("Uninstalling library %s."s, *it);
		}
		try
		{
			_codeCache.uninstallLibrary(*it);
		}
		catch (Poco::Exception& exc)
		{
			_logger.warning("Library %s of bundle %s cannot be removed from the code cache and stays there: %s"s,
				*it, pBundle->symbolicName(), exc.displayText());
		}
	}
}


void BundleLoader::listLibraries(Bundle* pBundle, std::vector<std::string>& list)
{
	pBundle->storage().list(libraryDirectory().toString(Path::PATH_UNIX), list);

	const auto itInvalid = std::remove_if(list.begin(), list.end(), [this, pBundle](const std::string& name)
	{
		if (BundleManifest::isValidFileName(name)) return false;
		_logger.warning("Library '%s' of bundle %s is ignored: its name cannot be used as a file name in the code cache."s,
			name, pBundle->symbolicName());
		return true;
	});
	list.erase(itInvalid, list.end());
}


Path BundleLoader::libraryDirectory() const
{
	Path p(false);
	p.pushDirectory("bin");
	p.pushDirectory(_osName);
	p.pushDirectory(_osArch);
	return p;
}


const std::string& BundleLoader::libraryNameFor(Bundle* pBundle)
{
	if (!pBundle->activatorLibrary().empty())
		return pBundle->activatorLibrary();
	else
		return pBundle->symbolicName();
}


void BundleLoader::sortBundles(std::vector<Bundle::Ptr>& bundles) const
{
	std::vector<Bundle::Ptr> remaining;
	listBundles(remaining);

	bundles.clear();
	bundles.reserve(remaining.size());
	// account for run level
	std::sort(remaining.begin(), remaining.end(), RunLevelLess());

	// A resolved bundle may require a bundle that has been uninstalled
	// or unloaded since; such a dependency does not delay it.
	std::set<std::string> loaded;
	for (const auto& pBundle: remaining)
	{
		loaded.insert(pBundle->symbolicName());
	}

	std::set<std::string> sorted;
	std::vector<Bundle::Ptr>::iterator it = remaining.begin();
	while (it != remaining.end())
	{
		bool hasDeps = false;
		if ((*it)->isResolved())
		{
			const Bundle::ResolvedDependencies& deps = (*it)->resolvedDependencies();
			for (Bundle::ResolvedDependencies::const_iterator itDep = deps.begin(); itDep != deps.end(); ++itDep)
			{
				if (loaded.contains(itDep->symbolicName) && !sorted.contains(itDep->symbolicName))
				{
					hasDeps = true;
					break;
				}
			}
		}
		if (!hasDeps)
		{
			bundles.push_back(*it);
			sorted.insert((*it)->symbolicName());
			remaining.erase(it);
			it = remaining.begin();
		}
		else ++it;
	}
	// The bundles left require each other; keep their run level order.
	bundles.insert(bundles.end(), remaining.begin(), remaining.end());
	std::reverse(bundles.begin(), bundles.end());
}


std::string BundleLoader::libraryPathFor(Bundle* pBundle)
{
	return _codeCache.pathFor(libraryNameFor(pBundle));
}


void BundleLoader::makeValidFileName(std::string& name)
{
	for (std::string::iterator it = name.begin(); it != name.end(); ++it)
	{
		if (!(Poco::Ascii::isAlphaNumeric(*it) || *it == '-')) *it = '_';
	}
}


bool BundleLoader::isProvidingModule(Bundle::Ptr pBundle, const BundleManifest::Dependency& module)
{
	Bundle::Modules modules = pBundle->providedModules();
	for (Bundle::Modules::const_iterator it = modules.begin(); it != modules.end(); ++it)
	{
		if (module.symbolicName == it->symbolicName && (module.versions.isEmpty() || module.versions.isInRange(it->version)))
			return true;
	}
	return false;
}


std::string BundleLoader::pathForLibrary(const std::string& libraryName)
{
	return _codeCache.pathFor(libraryName);
}


int BundleLoader::nextBundleId()
{
	int nextBundleId;
	{
		Poco::Mutex::ScopedLock lock(_mutex);
		nextBundleId = _nextBundleId++;
	}
	return nextBundleId;
}


#if defined(POCO_OSP_STATIC)
void BundleLoader::registerBundleActivator(const std::string& className, BundleActivatorFactory* pFactory)
{
	if (!_bundleActivatorFactories.contains(className))
	{
		_bundleActivatorFactories[className] = pFactory;
	}
	else
	{
		delete pFactory;
		throw Poco::ExistsException("Factory for BundleActivator already registered", className);
	}
}
#endif


} // namespace Poco::OSP
