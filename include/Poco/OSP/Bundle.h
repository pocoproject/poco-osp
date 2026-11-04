//
// Bundle.h
//
// Library: OSP
// Package: Bundle
// Module:  Bundle
//
// Definition of the Bundle class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Bundle_INCLUDED
#define OSP_Bundle_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/OSP/BundleProperties.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/RefCountedObject.h"
#include "Poco/AutoPtr.h"
#include "Poco/Mutex.h"
#include <set>
#include <atomic>


namespace Poco::OSP {


class BundleActivator;
class BundleLoader;
class BundleEvents;


class OSP_API Bundle: public Poco::RefCountedObject
	/// This class represents a Bundle in OSP.
	///
	/// For every bundle directory or file found in
	/// the OSP bundle repository, a Bundle object
	/// is created. The bundle class stores the state
	/// of the Bundle and gives access to the bundle's
	/// manifest, configuration properties and resources.
	///
	/// The Bundle class also manages localized resources.
{
public:
	Bundle() = delete;
	Bundle(const Bundle&) = delete;
	Bundle& operator = (const Bundle&) = delete;

	using Ptr = Poco::AutoPtr<Bundle>;
	using ConstPtr = const Ptr;
	using Dependency = BundleManifest::Dependency;
	using Dependencies = BundleManifest::Dependencies;
	using Module = BundleManifest::Module;
	using Modules = BundleManifest::Modules;

	enum State
	{
		BUNDLE_INSTALLED,
		BUNDLE_UNINSTALLED,
		BUNDLE_RESOLVED,
		BUNDLE_STARTING,
		BUNDLE_ACTIVE,
		BUNDLE_STOPPING
	};

	struct ResolvedDependency
	{
		std::string symbolicName;
		Version version;
		int id = 0;
			/// The internal ID of the bundle resolved against. Tells a
			/// bundle from one that has replaced it under the same
			/// symbolic name and version.
	};
	using ResolvedDependencies = std::vector<ResolvedDependency>;

	struct ModuleProvider
	{
		Dependency module;
		std::vector<ResolvedDependency> bundles;
	};
	using ModuleProviders = std::vector<ModuleProvider>;

	[[nodiscard]] int id() const;
		/// Returns the internal ID of the bundle.
		///
		/// Note that internal IDs are only valid during
		/// the runtime of an OSP-based application and may
		/// change between restarts.
		///
		/// Bundle IDs are guaranteed to be unique within
		/// the scope of a BundleLoader. No assumptions shall
		/// be made that bundle IDs are given out in a certain
		/// order or that they are strictly consecutive.

	[[nodiscard]] State state() const;
		/// Returns the current state of the bundle.

	[[nodiscard]] const std::string& stateString() const;
		/// Returns the current state of the bundle as a string.

	[[nodiscard]] bool isResolved() const;
		/// Returns true iff the bundle has been successfully
		/// resolved.
		///
		/// In other words, the bundle's state must be one of
		/// BUNDLE_RESOLVED, BUNDLE_STARTING, BUNDLE_ACTIVE, BUNDLE_STOPPING.

	[[nodiscard]] bool isActive() const;
		/// Returns true iff the bundle's state is BUNDLE_ACTIVE.

	[[nodiscard]] bool isStarted() const;
		/// Returns true iff the bundle's state is BUNDLE_STARTING,
		/// BUNDLE_ACTIVE or BUNDLE_STOPPING.

	[[nodiscard]] std::string path() const;
		/// Returns the path to the bundle's directory
		/// or archive file.

	[[nodiscard]] const std::string& name() const;
		/// Returns the bundle's name.
		///
		/// If the name contains a reference to a property,
		/// (${<property>}) the reference is expanded first.

	[[nodiscard]] const std::string& symbolicName() const;
		/// Returns the symbolic name of the bundle.

	[[nodiscard]] const Version& version() const;
		/// Returns the bundle's version.

	[[nodiscard]] const std::string& vendor() const;
		/// Returns the bundle's vendor name, or
		/// an empty string if no vendor name
		/// has been specified in the pManifest->
		///
		/// If the vendor string contains a reference to a property,
		/// (${<property>}) the reference is expanded first.

	[[nodiscard]] const std::string& copyright() const;
		/// Returns the bundle's copyright information,
		/// or an empty string if no copyright information
		/// has been specified in the pManifest->
		///
		/// If the copyright string contains a reference to a property,
		/// (${<property>}) the reference is expanded first.

	[[nodiscard]] const std::string& activatorClass() const;
		/// Returns the class name of the bundle's activator,
		/// or an empty string if the manifest does not
		/// specify an activator.

	[[nodiscard]] const std::string& activatorLibrary() const;
		/// Returns the name of the library containing
		/// the bundle's activator class, or an empty string
		/// if the manifest does not specify an activator library.

	[[nodiscard]] BundleActivator* activator() const;
		/// If the bundle has been started and has an activator,
		/// returns a pointer to the bundle's activator.
		/// Otherwise, returns NULL. The pointer stays set
		/// after the bundle has been stopped. It is reset to
		/// NULL when the bundle is unloaded or uninstalled and
		/// the activator is destroyed. In a static build
		/// (POCO_OSP_STATIC), the activator is not destroyed, and
		/// the pointer stays set.

	[[nodiscard]] bool lazyStart() const;
		/// Returns true if lazy start has been specified for
		/// the bundle.

	[[nodiscard]] bool sealed() const;
		/// Returns true if the bundle is sealed and cannot be
		/// extended by an extension bundle.

	[[nodiscard]] bool preventUninstall() const;
		/// Returns true if the bundle must not be uninstalled
		/// at run-time.
		///
		/// This is mostly useful for bundles that cannot be
		/// cleanly stopped. An example are bundles that
		/// contain third-party libraries that, once loaded,
		/// cannot be unloaded cleanly.

	[[nodiscard]] const std::string& runLevel() const;
		/// Returns the bundle's run level.
		///
		/// If the bundle does not specify a run level,
		/// returns the default "999-user".

	[[nodiscard]] bool isExtensionBundle() const;
		/// Returns true iff the bundle is an extension bundle.
		///
		/// An extension bundle will "donate" all of its
		/// resources to the bundle it extends. This means
		/// that all properties defined in the extension
		/// bundle will become properties of the extended
		/// bundle. Also, all resources of the extension
		/// bundle will become available through
		/// getResource() and getLocalizedResource() on
		/// the extended bundle, which look in the extension
		/// bundles first (see getResource()).

	[[nodiscard]] Bundle::Ptr extendedBundle() const;
		/// If the bundle is an extension bundle, returns
		/// the extended bundle, or in other words,
		/// the bundle that this bundle extends.
		///
		/// Returns a null pointer if the bundle is not
		/// an extension bundle, or the extended bundle
		/// has not been loaded.

	[[nodiscard]] const Poco::Util::AbstractConfiguration& properties() const;
		/// Returns the bundle's properties, which are
		/// obtained from the bundle's "bundle.properties"
		/// file (and its optional localizations).

	[[nodiscard]] std::istream* getResource(const std::string& name) const;
		/// Creates and returns an input stream for reading
		/// the bundle's (non-localized) resource with the given
		/// name.
		///
		/// Resources are ordinary files stored within the
		/// bundle's directory or archive file.
		///
		/// If the bundle has been extended by extension bundles,
		/// the resource is looked for in the extension bundles
		/// first and then in the bundle itself, so an extension
		/// bundle can replace a resource of the bundle. The order
		/// in which several extension bundles are searched is
		/// unspecified.
		///
		/// Returns NULL if the resource does not exist.
		///
		/// The caller gets ownership of the input stream and
		/// must delete it when it's no longer needed.

	[[nodiscard]] std::istream* getLocalizedResource(const std::string& name) const;
		/// Creates and returns an input stream for reading
		/// the bundle's localized resource with the given
		/// name.
		///
		/// Resources are ordinary files stored within the
		/// bundle's directory or archive file.
		///
		/// Returns NULL if the resource does not exist.
		///
		/// The caller gets ownership of the input stream and
		/// must delete it when it's no longer needed.

	[[nodiscard]] std::istream* getLocalizedResource(const std::string& name, const LanguageTag& language) const;
		/// Creates and returns an input stream for reading
		/// the bundle's localized (for the given language)
		/// resource with the given name.
		///
		/// Resources are ordinary files stored within the
		/// bundle's directory or archive file.
		///
		/// The localized paths are tried from the most specific
		/// one to the plain name, each as getResource() looks it
		/// up, so a more specific resource of the bundle itself
		/// takes precedence over a less specific one of an
		/// extension bundle.
		///
		/// Returns NULL if the resource does not exist.
		///
		/// The caller gets ownership of the input stream and
		/// must delete it when it's no longer needed.

	void resolve();
		/// Resolves the bundle by verifying the availability
		/// of all required bundles. The bundle must be in
		/// BUNDLE_INSTALLED state. A sucessful resolve()
		/// puts the bundle in BUNDLE_RESOLVED state.
		///
		/// Calls of resolve(), start(), stop() and uninstall() of
		/// the same bundle are serialized.
		///
		/// Throws an exception if the bundle cannot be resolved.

	void start();
		/// Starts the bundle. The bundle's state must be
		/// BUNDLE_RESOLVED.
		///
		/// Calls of resolve(), start(), stop() and uninstall() of
		/// the same bundle are serialized. A required bundle that
		/// another thread is starting is waited for, not started
		/// a second time.
		///
		/// Puts the bundle in BUNDLE_STARTING state, loads
		/// the bundle's activator and invokes it, and
		/// finally puts the bundle in BUNDLE_ACTIVE state.

	void stop();
		/// Stops the bundle. The bundle's state must be
		/// BUNDLE_ACTIVE. Puts the bundle into BUNDLE_STOPPING
		/// state, stops the bundle activator and finally puts
		/// the bundle into BUNDLE_RESOLVED state.

	void uninstall();
		/// Uninstalls the bundle. The bundle must be in
		/// BUNDLE_INSTALLED or BUNDLE_RESOLVED state.
		///
		/// The bundle's file or directory is deleted only if it is
		/// in the primary path of the BundleRepository, the path
		/// that BundleRepository::installBundle() installs into by
		/// default. The file or directory of a bundle in another
		/// path, such as a development tree, is kept.

	[[nodiscard]] ResolvedDependencies resolvedDependencies() const;
		/// Returns a vector containing information about all
		/// bundles explicitly (through Require-Bundle) or
		/// implicitly (through Require-Module) required by
		/// this bundle.
		///
		/// A copy is returned: the BundleLoader replaces the
		/// resolved dependencies when the bundle is resolved
		/// again after a required bundle has been replaced,
		/// possibly while another thread is reading them.

	[[nodiscard]] const Dependencies& requiredBundles() const;
		/// Returns a vector containing information about
		/// all bundles required by this bundle.

	[[nodiscard]] const Dependencies& requiredModules() const;
		/// Returns a vector containing information about
		/// all modules required by this bundle.

	[[nodiscard]] const Modules& providedModules() const;
		/// Returns a vector containing information about
		/// all modules provided by this bundle.

	[[nodiscard]] ModuleProviders moduleProviders() const;
		/// Returns a vector of all required modules, with
		/// information about the bundles providing each
		/// module.

	[[nodiscard]] const BundleManifest& manifest() const;
		/// Returns a reference to the BundleManifest object.

	[[nodiscard]] const LanguageTag& language() const;
		/// Returns the language used for localization.
	
	[[nodiscard]] BundleStorage& storage();
		/// Returns a reference to the BundleStorage object.

	static const std::string MANIFEST_FILE;
	static const std::string PROPERTIES_FILE;
	static const std::string BUNDLE_INSTALLED_STRING;
	static const std::string BUNDLE_UNINSTALLED_STRING;
	static const std::string BUNDLE_RESOLVED_STRING;
	static const std::string BUNDLE_STARTING_STRING;
	static const std::string BUNDLE_ACTIVE_STRING;
	static const std::string BUNDLE_STOPPING_STRING;
	static const std::string BUNDLE_INVALID_STRING;

protected:
	void loadManifest();
		/// Loads the bundle's manifest file.

	void loadProperties();
		/// Loads the bundle's properties.

	void addProperties(const std::string& path);
		/// Adds the properties from the property file
		/// specified by path to the bundle's properties.

	[[nodiscard]] BundleEvents& events();
		/// Returns a reference to the BundleEvents object.

	void setActivator(BundleActivator* pActivator);
		/// Sets the BundleActivator.

	void addExtensionBundle(Bundle* pExtensionBundle);
		/// Adds an extension bundle.
		///
		/// An extension bundle will "donate" all of its
		/// resources to the bundle it extends. This means
		/// that all properties defined in the extension
		/// bundle will become properties of the extended
		/// bundle. Also, all resources of the extension
		/// bundle will become available through
		/// getResource() and getLocalizedResource() on
		/// the extended bundle, which look in the extension
		/// bundles first (see getResource()).

	void removeExtensionBundle(Bundle* pExtensionBundle);
		/// Removes an extension bundle.
		///
		/// The resources and properties of the extension bundle
		/// will no longer be available to the extended bundle.

	void addResolvedDependency(const ResolvedDependency& dependency);
		/// Adds a dependency to the vector of resolved dependencies,
		/// unless it's already been added before.

	void clearResolvedDependencies();
		/// Clears the vector of resolved dependencies.

	void setResolution(ResolvedDependencies& dependencies, ModuleProviders& providers);
		/// Replaces the resolved dependencies and module providers
		/// in one step, so that a concurrent reader sees either the
		/// previous or the new resolution, never a partial one.
		/// The arguments receive the previous values.

	Bundle(int id, BundleLoader& loader, BundleStorage::Ptr pStorage, const LanguageTag& language);
		/// Creates the Bundle and reads the manifest
		/// using the given BundleStorage object.
		///
		/// The bundle takes ownership of the BundleStorage object.

	~Bundle() override;
		/// Destroys the Bundle.

private:

	int                     _id;
	std::atomic<State>      _state;
	BundleLoader&           _loader;
	BundleStorage::Ptr      _pStorage;
	LanguageTag             _language;
	BundleManifest::Ptr     _pManifest;
	std::string             _name;
	std::string             _vendor;
	std::string             _copyright;
	BundleProperties::Ptr   _pProperties;
	std::atomic<BundleActivator*> _pActivator;
	std::set<Bundle::Ptr>   _extensionBundles;
	mutable Poco::FastMutex _extensionBundlesMutex;
	std::atomic<int>        _extensionCount;
	ResolvedDependencies    _resolvedDependencies;
	ModuleProviders         _moduleProviders;
	mutable Poco::FastMutex _resolutionMutex;
	Poco::Mutex             _lifecycleMutex;
		/// Serializes resolve(), start(), stop() and uninstall() of the
		/// bundle and BundleLoader::unloadBundle(). Taken before the
		/// BundleLoader's mutex and before the mutexes of the bundles
		/// it requires or extends.

	friend class BundleLoader;
	friend class BundleFactory;
};


//
// inlines
//
inline int Bundle::id() const
{
	return _id;
}


inline Bundle::State Bundle::state() const
{
	return _state;
}


inline bool Bundle::isResolved() const
{
	const State state = _state;
	return state == BUNDLE_RESOLVED
	    || state == BUNDLE_STARTING
	    || state == BUNDLE_ACTIVE
	    || state == BUNDLE_STOPPING;
}


inline bool Bundle::isActive() const
{
	return _state == BUNDLE_ACTIVE;
}


inline bool Bundle::isStarted() const
{
	const State state = _state;
	return state == BUNDLE_STARTING
	    || state == BUNDLE_ACTIVE
	    || state == BUNDLE_STOPPING;
}


inline std::string Bundle::path() const
{
	return _pStorage->path();
}


inline const std::string& Bundle::name() const
{
	return _name;
}


inline const std::string& Bundle::symbolicName() const
{
	return _pManifest->symbolicName();
}


inline const Version& Bundle::version() const
{
	return _pManifest->version();
}


inline const std::string& Bundle::vendor() const
{
	return _vendor;
}


inline const std::string& Bundle::copyright() const
{
	return _copyright;
}


inline const std::string& Bundle::activatorClass() const
{
	return _pManifest->activatorClass();
}


inline const std::string& Bundle::activatorLibrary() const
{
	return _pManifest->activatorLibrary();
}


inline BundleActivator* Bundle::activator() const
{
	return _pActivator;
}


inline bool Bundle::sealed() const
{
	return _pManifest->sealed();
}


inline bool Bundle::preventUninstall() const
{
	return _pManifest->preventUninstall();
}


inline bool Bundle::lazyStart() const
{
	return _pManifest->lazyStart();
}


inline const std::string& Bundle::runLevel() const
{
	return _pManifest->runLevel();
}


inline const Poco::Util::AbstractConfiguration& Bundle::properties() const
{
	return *_pProperties;
}


inline const Bundle::Dependencies& Bundle::requiredBundles() const
{
	return _pManifest->requiredBundles();
}


inline const Bundle::Dependencies& Bundle::requiredModules() const
{
	return _pManifest->requiredModules();
}


inline const Bundle::Modules& Bundle::providedModules() const
{
	return _pManifest->providedModules();
}


inline Bundle::ResolvedDependencies Bundle::resolvedDependencies() const
{
	Poco::FastMutex::ScopedLock lock(_resolutionMutex);
	return _resolvedDependencies;
}


inline Bundle::ModuleProviders Bundle::moduleProviders() const
{
	Poco::FastMutex::ScopedLock lock(_resolutionMutex);
	return _moduleProviders;
}


inline const BundleManifest& Bundle::manifest() const
{
	return *_pManifest;
}


inline const LanguageTag& Bundle::language() const
{
	return _language;
}


inline BundleStorage& Bundle::storage()
{
	return *_pStorage;
}


} // namespace Poco::OSP


#endif // OSP_Bundle_INCLUDED
