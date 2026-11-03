//
// BundleLoader.h
//
// Library: OSP
// Package: Bundle
// Module:  BundleLoader
//
// Definition of the BundleLoader class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_BundleLoader_INCLUDED
#define OSP_BundleLoader_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/BasicEvent.h"
#include "Poco/ClassLoader.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"
#include "Poco/Logger.h"
#if defined(POCO_OSP_STATIC)
#include "Poco/Instantiator.h"
#endif
#include <map>
#include <optional>
#include <set>
#include <vector>


namespace Poco::OSP {


class CodeCache;


class OSP_API BundleLoader
	/// The BundleLoader is used internally by the framework
	/// to manage Bundle and BundleContext objects, and to
	/// load shared libraries containing BundleActivator classes.
	///
	/// The BundleLoader uses the logger osp.core.BundleLoader to
	/// write diagnostic messages.
{
public:
	using ActivatorClassLoader = Poco::ClassLoader<BundleActivator>;
	using ActivatorClassLoaderPtr = Poco::SharedPtr<ActivatorClassLoader>;

	struct BundleInfo
	{
		Bundle::Ptr             pBundle;
		BundleContext::Ptr      pContext;
		ActivatorClassLoaderPtr pClassLoader;
		std::optional<bool>     deleteOnUninstall;
	};
	using BundleMap = std::map<std::string, BundleInfo>;
	using BundleSet = std::set<Bundle*>;

	struct BundleError
	{
		Bundle::Ptr pBundle;
		Bundle::State targetState;
		Poco::Exception* pException;
	};

	Poco::BasicEvent<const BundleError> bundleError;
		/// Fired when an error occurs while resolving,
		/// starting or stopping a bundle.

	BundleLoader(CodeCache& codeCache, BundleFactory::Ptr pBundleFactory, BundleContextFactory::Ptr pBundleContextFactory, const std::string& osName, const std::string& osArch, bool autoUpdateCodeCache = true);
		/// Creates the BundleLoader.

	BundleLoader(CodeCache& codeCache, BundleFactory::Ptr pBundleFactory, BundleContextFactory::Ptr pBundleContextFactory, bool autoUpdateCodeCache = true);
		/// Creates the BundleLoader.

	~BundleLoader();
		/// Destroys the BundleLoader.

	BundleLoader() = delete;
	BundleLoader(const BundleLoader&) = delete;
	BundleLoader& operator = (const BundleLoader&) = delete;

	[[nodiscard]] Bundle::ConstPtr findBundle(const std::string& symbolicName) const;
		/// Returns the Bundle object for the bundle with
		/// the given symbolic name, or NULL if the
		/// bundle does not exist.

	[[nodiscard]] Bundle::ConstPtr findBundle(int id) const;
		/// Returns the Bundle object for the bundle with
		/// the given ID, or NULL if the
		/// bundle does not exist.

	[[nodiscard]] Bundle::Ptr createBundle(const std::string& path);
		/// Loads a bundle from the given path.
		///
		/// A new Bundle object is created for the bundle,
		/// and the Bundle's state is BUNDLE_INSTALLED.
		/// The bundle is not added to the bundle map.

	Bundle::Ptr loadBundle(const std::string& path);
		/// Loads a bundle from the given path.
		/// A new Bundle object is created for the bundle,
		/// and the Bundle's state is BUNDLE_INSTALLED.
		///
		/// The bundle is then added to the bundle map.
		/// If another version of the same bundle is already
		/// loaded, a BundleVersionConflictException is thrown.
		///
		/// Returns the new Bundle object.

	void loadBundle(Bundle::Ptr pBundle);
		/// Loads the given Bundle, which must have
		/// been created with createBundle().
		///
		/// The bundle is then added to the bundle map.
		/// If another version of the same bundle is already
		/// loaded, a BundleVersionConflictException is thrown.

	void unloadBundle(Bundle::Ptr pBundle);
		/// Unloads the given bundle.
		///
		/// The bundle is removed from the bundle map and
		/// its activator library, if loaded, is unloaded.

	void listBundles(std::vector<Bundle::Ptr>& bundles) const;
		/// Fills the given vector with all bundles
		/// known to the loader.

	void listBundles(std::vector<Bundle::Ptr>& bundles, BundleFilter::Ptr pFilter) const;
		/// Fills the given vector with all bundles
		/// known to the loader, filtered by the
		/// given BundleFilter.

	void listRequiringBundles(Bundle::ConstPtr pBundle, std::vector<Bundle::Ptr>& bundles) const;
		/// Fills the given vector with the resolved bundles that
		/// require the given bundle, directly or through other
		/// bundles, in the order in which they can be stopped:
		/// a bundle comes before the bundles it requires.

	void resolveAllBundles();
		/// Resolves all bundles.
		///
		/// If a bundle cannot be resolved, an error is logged
		/// and the loader will continue to resolve other bundles.

	void startAllBundles();
		/// Starts all bundles, ordered by their run level.
		/// If a bundle has the lazyStart property set,
		/// that bundle won't be started unless another bundle
		/// requires it.
		///
		/// Bundles with lower run levels will be started
		/// before bundles with higher run levels.
		///
		/// If a bundle cannot be started, an error will be
		/// logged, and the loader will continue to load
		/// other bundles.

	void stopAllBundles();
		/// Stops all bundles.
		///
		/// Bundles are stopped in order of their depedencies.
		/// A bundle that fails to stop is logged and reported
		/// with the bundleError event, and the remaining bundles
		/// are stopped.

	void unloadAllBundles(bool unloadLibraries = true);
		/// Stops and unloads all bundles. If unloadLibraries is
		/// false, activators are destroyed and bundle data structures
		/// are cleaned up in dependency order, but shared libraries
		/// are not unloaded from the process. This avoids crashes in
		/// libraries with non-trivial static destruction (e.g. V8)
		/// during process shutdown.
		///
		/// A bundle that fails to stop is reported as with
		/// stopAllBundles() and stays loaded.

	void setDeleteOnUninstall(Bundle::ConstPtr pBundle, bool deleteOnUninstall);
		/// Sets whether uninstalling the given bundle deletes its
		/// file or directory. The BundleRepository sets this for the
		/// bundles in its primary path.
		///
		/// By default, the file or directory is kept, and an information
		/// message says so. If false has been set, it is kept without the
		/// message: the caller takes care of it, as the BundleRepository
		/// does for a bundle that it replaces.
		///
		/// Throws a BundleException if the bundle is not loaded.

	[[nodiscard]] BundleEvents& events();
		/// Returns a reference to the BundleEvents
		/// object.

	[[nodiscard]] const std::string& osName() const;
		/// Returns the name of the operating system.

	[[nodiscard]] const std::string& osArchitecture() const;
		/// Returns the name of the hardware architecture.

	[[nodiscard]] std::string pathForLibrary(const std::string& libraryName);
		/// Returns the full path for the library with the given
		/// libraryName. If libraryName is the empty string,
		/// return the path to the code cache directory.

	BundleContext::Ptr contextForBundle(Bundle::ConstPtr pBundle) const;
		/// Returns the BundleContext for the given bundle.

	int nextBundleId();
		/// Returns a new unique bundle ID.

#if defined(POCO_OSP_STATIC)
	using BundleActivatorFactory = Poco::AbstractInstantiator<BundleActivator>;

	void registerBundleActivator(const std::string& className, BundleActivatorFactory* pFactory);
		/// Registers a factory object for the BundleActivator with the given class name.
		/// The BundleLoader takes ownership of the factory and deletes it in its destructor.
		///
		/// This is only available if OSP is compiled with the preprocessor symbol
		/// POCO_OSP_STATIC defined. In this case, dynamic loading of BundleActivator instances
		/// from shared libraries is disabled, and all BundleActivator instances will be
		/// created using statically registered factories.
		///
		/// This is only useful on platforms that do not support loading of shared libraries
		/// (such as the iPhone, where, although shared libraries are supported by the system,
		/// application distribution policies disallow loading of shared libraries).

	template <class BA>
	void registerBundleActivator(const std::string& className)
	{
		registerBundleActivator(className, new Poco::Instantiator<BA, Poco::OSP::BundleActivator>());
	}
#endif

protected:
	void resolveBundle(Bundle* pBundle);
		/// Resolves the given bundle.
		///
		/// The bundles it requires are resolved on the way, with
		/// _mutex held. Their bundleResolving and bundleResolved
		/// events are fired in the order in which they occurred,
		/// once _mutex has been released.

	void resolveBundleImpl(Bundle* pBundle);
		/// Resolves the given bundle. Requires _mutex to be held.

	void fireResolveEvent(BundleEvent& event);
		/// Fires the given bundleResolving or bundleResolved event.
		/// While resolveBundle() holds _mutex in the calling thread,
		/// the event is collected and fired by resolveBundle().

	[[nodiscard]] Bundle::ModuleProviders resolveModules(Bundle* pBundle) const;
		/// Determines the bundles providing the required modules.

	void resolveDependencies(Bundle* pBundle, const BundleManifest::Dependencies& deps, Bundle::ResolvedDependencies* pResolved = nullptr);
		/// Resolves all direct dependencies of the given bundle.
		///
		/// The resolved dependencies are added to pResolved if given,
		/// to the bundle otherwise.

	void resolveProviders(Bundle* pBundle, const Bundle::ModuleProviders& providers, Bundle::ResolvedDependencies* pResolved = nullptr);
		/// Resolves the module providers for the given bundle.
		///
		/// The resolved dependencies are added to pResolved if given,
		/// to the bundle otherwise.

	void startBundle(Bundle* pBundle);
		/// Starts the given bundle.

	class StartingBundle
		/// Records, for the calling thread, the bundle whose start is
		/// in progress, for the error message of startAllBundles().
		///
		/// When the start succeeds, the bundle recorded before is
		/// recorded again. When it fails, the record is kept: it names
		/// the bundle whose start has failed, which is a required bundle
		/// if the start of that one has failed.
	{
	public:
		StartingBundle() = delete;
		POCO_NODISCARD_CTOR explicit StartingBundle(const Bundle& bundle);
		StartingBundle(const StartingBundle&) = delete;
		StartingBundle(StartingBundle&&) = delete;
		~StartingBundle();

		StartingBundle& operator = (const StartingBundle&) = delete;
		StartingBundle& operator = (StartingBundle&&) = delete;

	private:
		std::string _previous;
		int _uncaughtExceptions;
	};

	void stopBundle(Bundle* pBundle);
		/// Stops the given bundle.

	void uninstallBundle(Bundle* pBundle);
		/// Uninstalls the given bundle. Its file or directory is
		/// deleted if setDeleteOnUninstall() has set true for it.

	void resolveDependency(Bundle* pBundle, const BundleManifest::Dependency& dependency, Bundle::ResolvedDependencies* pResolved = nullptr);
		/// Resolves a single dependency of a bundle.
		///
		/// The resolved dependency is added to pResolved if given,
		/// to the bundle otherwise.

	[[nodiscard]] bool isResolving(Bundle* pBundle) const;
		/// Returns true iff the given bundle
		/// is currently being resolved.

	void startDependencies(Bundle* pBundle);
		/// Starts all bundles that the given
		/// bundle requires to run.

	[[nodiscard]] bool hasStaleDependencies(const Bundle* pBundle) const;
		/// Returns true iff a bundle the given bundle has been
		/// resolved against is no longer loaded, or has been
		/// replaced by another bundle - be it a different version
		/// or another build of the same version, and no matter
		/// whether the replacement has been resolved already.

	void unresolvedDependencies(Bundle* pBundle, std::vector<Bundle::Ptr>& unresolved) const;
		/// Appends the bundles resolving the given bundle would
		/// have to resolve first: required bundles within the
		/// requested version range and module providers that are
		/// installed, but not resolved yet.
		///
		/// Throws a BundleResolveException if a required module
		/// is not available.

	void refreshResolvedDependencies(Bundle* pBundle);
		/// Resolves the dependencies of an already resolved bundle
		/// again if hasStaleDependencies() returns true for it.
		///
		/// Resolved dependencies are pinned to the bundle they were
		/// resolved against. After a required bundle has been
		/// replaced (upgrade at runtime), the dependent bundle can
		/// be started again as long as the replacement satisfies
		/// the version range in its manifest.
		///
		/// Must be called without holding _mutex: the replacements
		/// are resolved first, which fires their resolve events,
		/// and event delegates call back into the BundleLoader.
		///
		/// The new resolution replaces the previous one in one step.
		/// Throws a BundleResolveException, leaving the previous
		/// resolution in place, if the dependencies can no longer
		/// be satisfied.

	void resolveAgain(Bundle* pBundle);
		/// Resolves the given, already resolved bundle against the
		/// bundles available now. Requires _mutex to be held, and
		/// unresolvedDependencies() to come up empty, so that no
		/// other bundle is resolved on the way.

	BundleActivator* loadActivator(BundleInfo& bundleInfo);
		/// Loads the activator for the given bundle,
		/// unless it has already been loaded.

	void unloadActivator(BundleInfo& bundleInfo);
		/// Unloads the activator for the given bundle.

	void installLibraries(Bundle* pBundle);
		/// Copies all shared libraries in the bundle
		/// (for the current platform) to the code cache.

	void uninstallLibraries(Bundle* pBundle);
		/// Removes all shared libraries belonging
		/// to the given bundle from the code cache.

	void installLibrary(Bundle* pBundle, const Poco::Path& p, const Poco::Timestamp* pTS = nullptr);
		/// Copies the library to the code cache.
		/// If timestamp is provided, the file creation
		/// date is modified to the given time.

	[[nodiscard]] const std::string& libraryNameFor(Bundle* pBundle);
		/// Returns the library name for the library
		/// containing the activator for the given bundle.

	void listLibraries(Bundle* pBundle, std::vector<std::string>& list);
		/// Lists all shared libraries contained in the bundle
		/// for the current platform.

	void sortBundles(std::vector<Bundle::Ptr>& bundles) const;
		/// Sorts all bundles topologically, based on
		/// their dependencies.
		///
		/// The bundles are sorted in such a way so that
		/// bundles can be safely stopped in the resulting
		/// order.

	[[nodiscard]] std::string libraryPathFor(Bundle* pBundle);
		/// Returns the library path for the library
		/// containing the activator for the given bundle.

	static void makeValidFileName(std::string& name);
		/// Replaces all non alphanumeric characters in name
		/// with an underscore.

	[[nodiscard]] static bool isProvidingModule(Bundle::Ptr pBundle, const BundleManifest::Dependency& module);
		/// Checks whether the given bundle provides the given module.

	void handleStartError(Bundle::Ptr pBundle, const std::string& errorMsg, Poco::Exception& exc);
		/// Logs a bundle start error and fires the bundleError event.

	void stopBundleOnShutdown(Bundle::Ptr pBundle);
		/// Stops the bundle. If stopping fails, reports the error
		/// with handleStopError(); the bundle is then still active.

	void handleStopError(Bundle::Ptr pBundle, Poco::Exception& exc);
		/// Logs a bundle stop error and fires the bundleError event.

private:

	using BundleIdMap = std::map<int, Bundle::Ptr>;

	int                       _nextBundleId;
	CodeCache&                _codeCache;
	bool                      _autoUpdateCodeCache;
	BundleFactory::Ptr        _pBundleFactory;
	BundleContextFactory::Ptr _pBundleContextFactory;
	BundleEvents              _events;
	std::string               _osName;
	std::string               _osArch;
	BundleMap                 _bundles;
	BundleIdMap               _bundleIds;
	BundleSet                 _resolvingBundles;
	int                       _resolveDepth = 0;
	std::vector<BundleEvent>  _deferredResolveEvents;
	Poco::Logger&             _logger;
	mutable Poco::Mutex       _mutex;

#if defined(POCO_OSP_STATIC)
	using BundleActivatorFactoryMap = std::map<std::string, BundleActivatorFactory*>;

	BundleActivatorFactoryMap _bundleActivatorFactories;
#endif

	friend class Bundle;
};


//
// inline
//
inline BundleEvents& BundleLoader::events()
{
	return _events;
}


inline const std::string& BundleLoader::osName() const
{
	return _osName;
}


inline const std::string& BundleLoader::osArchitecture() const
{
	return _osArch;
}


} // namespace Poco::OSP


#endif // OSP_BundleLoader_INCLUDED
