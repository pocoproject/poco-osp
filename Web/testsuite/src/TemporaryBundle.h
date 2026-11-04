//
// TemporaryBundle.h
//
// Definition of the TemporaryBundle class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TemporaryBundle_INCLUDED
#define TemporaryBundle_INCLUDED


#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <memory>
#include <string>


class TemporaryBundle
	/// Bundles in a directory with a unique name below the temporary directory,
	/// loaded by a bundle loader that has its own service registry. The directory
	/// is removed when the TemporaryBundle is destroyed.
	///
	/// The loader has loaded the directory bundle "osp.web.test.dir" and the ZIP
	/// bundle "osp.web.test.zip", which both contain the web resources
	/// www/hello.txt (HELLO_TXT) and www/index.html (INDEX_HTML). The bundle
	/// "osp.web.test.unloaded" is created but not loaded, so it has no persistent
	/// directory; its bundle.properties maps the resource directory persist,
	/// which contains file.txt, to the persistent directory in strict mode.
{
public:
	TemporaryBundle() = delete;

	explicit TemporaryBundle(const std::string& properties);
		/// Creates the bundles, with the given content of the bundle.properties
		/// file of the directory bundle.

	~TemporaryBundle();
		/// Destroys the services in the registry, then the bundles and the loader,
		/// and removes the directory.

	TemporaryBundle(const TemporaryBundle&) = delete;
	TemporaryBundle(TemporaryBundle&&) = delete;
	TemporaryBundle& operator = (const TemporaryBundle&) = delete;
	TemporaryBundle& operator = (TemporaryBundle&&) = delete;

	[[nodiscard]] Poco::OSP::Bundle::Ptr bundle() const;
		/// Returns the directory bundle.

	[[nodiscard]] Poco::OSP::Bundle::Ptr zipBundle() const;
		/// Returns the ZIP bundle.

	[[nodiscard]] Poco::OSP::Bundle::Ptr unloadedBundle() const;
		/// Returns the bundle that is not loaded.

	[[nodiscard]] Poco::OSP::BundleContext::Ptr context() const;
		/// Returns the context of the directory bundle.

	[[nodiscard]] Poco::OSP::ServiceRegistry& registry();
		/// Returns the service registry of the bundle contexts.

	[[nodiscard]] Poco::OSP::BundleEvents& events();
		/// Returns the bundle events of the loader.

	static void writeFile(const Poco::Path& baseDir, const std::string& relativePath, const std::string& content);
		/// Writes content to the file with the given path (in UNIX notation)
		/// below baseDir, and creates the directories of the path.

	static const std::string HELLO_TXT;
		/// The content of www/hello.txt.

	static const std::string INDEX_HTML;
		/// The content of www/index.html.

private:
	Poco::TemporaryFile _directory;
	Poco::OSP::SystemEvents _systemEvents;
	std::unique_ptr<Poco::OSP::CodeCache> _pCodeCache;
	Poco::OSP::BundleContextFactory::Ptr _pBundleContextFactory;
	std::unique_ptr<Poco::OSP::BundleLoader> _pLoader;
	Poco::OSP::Bundle::Ptr _pBundle;
	Poco::OSP::Bundle::Ptr _pZipBundle;
	Poco::OSP::Bundle::Ptr _pUnloadedBundle;
	Poco::OSP::BundleContext::Ptr _pContext;
	// Declared last, so that the services that tests register are destroyed first:
	// session managers and dispatchers unsubscribe from the bundle events of the
	// loader when they are destroyed.
	Poco::OSP::ServiceRegistry _registry;
};


//
// inlines
//
inline Poco::OSP::Bundle::Ptr TemporaryBundle::bundle() const
{
	return _pBundle;
}


inline Poco::OSP::Bundle::Ptr TemporaryBundle::zipBundle() const
{
	return _pZipBundle;
}


inline Poco::OSP::Bundle::Ptr TemporaryBundle::unloadedBundle() const
{
	return _pUnloadedBundle;
}


inline Poco::OSP::BundleContext::Ptr TemporaryBundle::context() const
{
	return _pContext;
}


inline Poco::OSP::ServiceRegistry& TemporaryBundle::registry()
{
	return _registry;
}


inline Poco::OSP::BundleEvents& TemporaryBundle::events()
{
	return _pLoader->events();
}


#endif // TemporaryBundle_INCLUDED
