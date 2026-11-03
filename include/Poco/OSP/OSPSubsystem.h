//
// OSPSubsystem.h
//
// Library: OSP
// Package: Util
// Module:  OSPSubsystem
//
// Definition of the OSPSubsystem class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_OSPSubsystem_INCLUDED
#define OSP_OSPSubsystem_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/Util/Subsystem.h"
#include <memory>


namespace Poco::OSP {


class CodeCache;
class ServiceRegistry;
class BundleLoader;
class BundleRepository;


class OSP_API OSPSubsystem: public Poco::Util::Subsystem
	/// This subsystem implementation sets up the
	/// OSP runtime environment, including
	/// BundleLoader, CodeCache and ServiceRegistry.
	///
	/// It also defines the "clean" command line option,
	/// which forces the code cache to be cleared, and
	/// the "bundles" command line option which allows
	/// for specification of additional bundle repositories.
	///
	/// The following configuration properties are used:
	///   - osp.bundleRepository:    list of directories containing bundles
	///                              (defaults to ${application.dir}bundles)
	///   - osp.codeCache:           location of code cache directory
	///                              (defaults to ${application.dir}codeCache)
	///   - osp.autoUpdateCodeCache: automatically update shared libraries in codeCache
	///                              with newer versions found in bundles
	///   - osp.sharedCodeCache:     allow using the same code cache directory for
	///                              multiple processes, causing certain operations
	///                              to be guarded by a global lock
	///   - osp.language:            language used for localization (overrides
	///                              the system default)
	///   - osp.data                 the directory where temporary and persistent
	///                              data for bundles is stored (defaults to
	///                              ${application.dir}data)
	///   - osp.skipBundles:         comma/semicolon-separated list of glob
	///                              patterns matching bundle symbolic names
	///                              to exclude from loading; ignored if a
	///                              bundle filter has been set with
	///                              setBundleFilter()
	///
	/// The following configuration properties are set:
	///   - osp.version: OSP Version from osp.core bundle (only if osp.core bundle is present)
	///   - osp.osname:  name of operating system
	///   - osp.osarch:  hardware architecture identifier
{
public:
	OSPSubsystem();
		/// Creates the OSPSubsystem.

	~OSPSubsystem() override;
		/// Destroys the OSPSubsystem.
	
	[[nodiscard]] BundleLoader& bundleLoader();
		/// Returns a reference to the bundle loader.
		
	[[nodiscard]] ServiceRegistry& serviceRegistry();
		/// Returns a reference to the service registry.

	void cancelInit();
		/// Cancels initialization.	

	void setBundleFilter(BundleFilter::Ptr pFilter);
		/// Sets the BundleFilter to be used by the BundleRepository.
		/// May be set to a null pointer to disable filtering.
		///
		/// Must be called before initialize().
		
	[[nodiscard]] BundleFilter::Ptr getBundleFilter() const;
		/// Returns the BundleFilter used by the BundleRepository.
		/// May return a null pointer if no BundleFilter has been set.

	// Subsystem
	void initialize(Poco::Util::Application& app) override;
	void uninitialize() override;
	void defineOptions(Poco::Util::OptionSet& options) override;
	[[nodiscard]] const char* name() const override;

protected:
	void handleClear(const std::string& name, const std::string& value);
	void handleBundles(const std::string& name, const std::string& value);

	virtual void loadBundles(Poco::Util::Application& app);
		/// Loads all bundles from all known bundle repositories.

	virtual void startBundles(Poco::Util::Application& app);
		/// Resolves and starts all loaded bundles.
	
private:
	std::unique_ptr<CodeCache>        _pCodeCache;
	std::unique_ptr<ServiceRegistry>  _pServiceRegistry;
	std::unique_ptr<BundleLoader>     _pBundleLoader;
	std::unique_ptr<BundleRepository> _pBundleRepository;
	BundleFilter::Ptr _pBundleFilter;
	SystemEvents      _systemEvents;
	bool              _clearCache = false;
	bool              _cancelInit = false;
	std::string       _bundles;
};


//
// inlines
//
inline BundleLoader& OSPSubsystem::bundleLoader()
{
	poco_check_ptr(_pBundleLoader.get());
	
	return *_pBundleLoader;
}


inline ServiceRegistry& OSPSubsystem::serviceRegistry()
{
	poco_check_ptr (_pServiceRegistry.get());
	
	return *_pServiceRegistry;
}


} // namespace Poco::OSP


#endif // OSP_OSPSubsystem_INCLUDED
