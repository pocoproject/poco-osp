//
// BundleRepository.h
//
// Library: OSP
// Package: Bundle
// Module:  BundleRepository
//
// Definition of the BundleRepository class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_BundleRepository_INCLUDED
#define OSP_BundleRepository_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include <vector>
#include <map>
#include <set>
#include <istream>


namespace Poco::OSP {


class BundleLoader;


class OSP_API BundleRepository
	/// The bundle repository manages one or more directories
	/// in the file system containing bundles.
	///
	/// The optional BundleFilter applies to the bundles loaded by
	/// loadBundles() and to the bundles installed with installBundle().
	/// installBundle() throws a BundleInstallException for a bundle
	/// that the filter rejects.
	///
	/// Uninstalling a bundle (Bundle::uninstall()) deletes its file
	/// or directory only if it is in the primary path, whether it
	/// has been installed there or loaded from there. The file or
	/// directory of a bundle in a secondary path, such as a
	/// development tree, is kept; this includes a bundle installed
	/// into a secondary path with installBundle(repositoryPath, ...).
	///
	/// An install or replacement of a bundle fails with a
	/// BundleInstallException while another install or replacement
	/// of a bundle with the same symbolic name, or of the replaced
	/// bundle, is in progress. Installs of other bundles run
	/// concurrently; an activator may install a bundle.
	///
	/// The BundleRepository uses the Logger "osp.core.BundleRepository"
	/// to log non-fatal errors.
{
public:
	BundleRepository(const std::string& path, BundleLoader& loader, BundleFilter::Ptr pBundleFilter = nullptr);
		/// Creates the BundleRepository, using the given
		/// path and BundleLoader, and optional BundleFilter.
		///
		/// The path argument may contain a single path, or a list
		/// of paths, separated by the platform's path separator
		/// (':' on Unix platforms, ';' on Windows).
		///
		/// The first path given in a list is said to be the
		/// primary path. Other paths are secondary paths.
		///
		/// Paths may either reference a directory containing
		/// bundles, or a bundle file (or bundle directory with
		/// the extension ".bndl") directly.
		///
		/// Paths may contain Glob expressions (see Poco::Glob).

	BundleRepository(const std::vector<std::string>& paths, BundleLoader& loader,
		BundleFilter::Ptr pBundleFilter = nullptr);
		/// Creates the BundleRepository, using the given
		/// paths and BundleLoader, and optional BundleFilter.
		///
		/// The paths argument specifies a list of paths where
		/// bundles are searched.
		///
		/// The first path given in a list is said to be the
		/// primary path. Other paths are secondary paths.
		///
		/// Paths may either reference a directory containing
		/// bundles, or a bundle file (or bundle directory with
		/// the extension ".bndl") directly.
		///
		/// Paths may contain Glob expressions (see Poco::Glob).

	~BundleRepository();
		/// Destroys the BundleRepository.

	BundleRepository() = delete;
	BundleRepository(const BundleRepository&) = delete;
	BundleRepository& operator = (const BundleRepository&) = delete;

	void loadBundles();
		/// Loads all available bundles, using the BundleLoader.
		///
		/// Temporary files of installs that did not complete
		/// are skipped.
		///
		/// If two or more versions of a bundle are found,
		/// the latest version of the bundle is loaded
		/// and a warning message is logged. Of two bundles
		/// with the same version, the one found last is loaded:
		/// the paths are searched in the given order, and the
		/// entries of a directory in the order of their names.

	Bundle::Ptr installBundle(std::istream& istr);
		/// Reads a bundle archive file from the given stream
		/// and installs it in the primary path.
		///
		/// The bundle's file name in the bundle repository
		/// will be "<symbolicName>_<version>.bndl".
		///
		/// Returns the newly installed bundle, which will
		/// be in INSTALLED state.

	Bundle::Ptr installBundle(const std::string& repositoryPath, std::istream& istr);
		/// Reads a bundle archive file from the given stream
		/// and installs it in the specified repositoryPath.
		///
		/// The repositoryPath must be one of the bundle repository paths specified
		/// in the constructor, otherwise the bundle won't be found
		/// the next time the application starts.
		///
		/// The directory referenced by repositoryPath must exist and must
		/// be writable, otherwise installing the bundle will fail.
		///
		/// The bundle's file name in the bundle repository
		/// will be "<symbolicName>_<version>.bndl".
		///
		/// Returns the newly installed bundle, which will
		/// be in INSTALLED state.

	Bundle::Ptr installBundle(std::istream& istr, const std::string& replaceBundle);
		/// Reads a bundle archive file from the given stream
		/// and installs it in the primary path.
		///
		/// If a bundle with the symbolic name given in replaceBundle
		/// exists, the active bundles that require it are stopped,
		/// and the bundle is stopped and uninstalled before the new
		/// bundle is installed. If the replaced bundle is in the primary
		/// path or in the path that the new bundle is installed into, its
		/// file or directory is kept under a temporary name until the new
		/// bundle has been loaded. If the new bundle cannot be installed,
		/// the replaced bundle is loaded again, in INSTALLED state. In both
		/// cases, the bundles that have been stopped are started again.
		///
		/// The bundle's file name in the bundle repository
		/// will be "<symbolicName>_<version>.bndl".
		///
		/// Returns the newly installed bundle, which will
		/// be in INSTALLED state.

	Bundle::Ptr installBundle(const std::string& repositoryPath, std::istream& istr, const std::string& replaceBundle);
		/// Reads a bundle archive file from the given stream
		/// and installs it in the primary repositoryPath.
		///
		/// The repositoryPath must be one of the bundle repository paths specified
		/// in the constructor, otherwise the bundle won't be found
		/// the next time the application starts.
		///
		/// The directory referenced by repositoryPath must exist and must
		/// be writable, otherwise installing the bundle will fail.
		///
		/// If a bundle with the symbolic name given in replaceBundle
		/// exists, the active bundles that require it are stopped,
		/// and the bundle is stopped and uninstalled before the new
		/// bundle is installed. If the replaced bundle is in the primary
		/// path or in the path that the new bundle is installed into, its
		/// file or directory is kept under a temporary name until the new
		/// bundle has been loaded. If the new bundle cannot be installed,
		/// the replaced bundle is loaded again, in INSTALLED state. In both
		/// cases, the bundles that have been stopped are started again.
		///
		/// When the new bundle has been loaded, the file or directory
		/// of the replaced bundle is deleted, also if repositoryPath is
		/// a secondary path, where an uninstall keeps it.
		///
		/// The bundle's file name in the bundle repository
		/// will be "<symbolicName>_<version>.bndl".
		///
		/// Returns the newly installed bundle, which will
		/// be in INSTALLED state.

	[[nodiscard]] const std::vector<std::string>& paths() const;
		/// Returns a vector containing all configured repository paths.

	static const std::string TEMP_BUNDLE_EXTENSION;
		/// The extension of the temporary file that installBundle()
		/// writes. loadBundles() skips files with this extension.

protected:
	using BundleMap = std::map<std::string, Bundle::Ptr>;

	struct RemovedBundle
		/// A bundle that installBundle() has removed to replace it.
	{
		std::vector<Bundle::Ptr> stoppedBundles;
			/// The active bundles requiring the removed bundle,
			/// in the order in which they have been stopped.
		std::string path;
			/// The path of the removed bundle, or empty if no
			/// bundle has been uninstalled.
		std::string backupPath;
			/// Where the file or directory of the removed bundle
			/// has been moved to, or empty if it has been kept.
		bool deleteOnUninstall = false;
			/// Whether uninstalling the bundle, once it has been
			/// loaded again, deletes its file or directory (see
			/// BundleLoader::setDeleteOnUninstall()).
	};

	void loadBundles(const std::string& path, BundleMap& bundles);
		/// Loads all availble bundles from the given path.

	void loadBundle(const std::string& path, BundleMap& bundles);
		/// Loads a bundle from the given path.

	Bundle::Ptr installBundleImpl(std::istream& istr, const std::string& path, const std::string& replaceBundle);
		/// Reads a bundle archive file from the given stream
		/// and installs it in the given path.

	[[nodiscard]] bool isReplacedBundle(const std::string& path, const std::string& replaceBundle) const;
		/// Returns true iff path is the file or directory of the loaded
		/// bundle with the symbolic name replaceBundle.

	[[nodiscard]] std::string tempBundleName(const std::string& directory);
		/// Returns a temporary bundle name in the form
		/// "tmp.bundle-nnnn.tmp" that no file or directory in
		/// the given directory has. Requires _installMutex
		/// to be held.

	[[nodiscard]] static bool isInRepository(const std::string& bundlePath, const std::string& repositoryPath);
		/// Returns true iff the bundle file or directory at bundlePath
		/// is an entry of the directory repositoryPath.

	void removeBundle(const std::string& symbolicName, const std::string& repositoryPath, RemovedBundle& removed);
		/// Removes the bundle for a replacement. The active bundles
		/// that require it are stopped, then the bundle is stopped
		/// (if necessary) and uninstalled. If the bundle is in the
		/// primary path or in repositoryPath, its file or directory
		/// is moved to a temporary name in the same directory, which
		/// loadBundles() skips.
		///
		/// What restoreBundle() needs is recorded in removed.

	void restoreBundle(const RemovedBundle& removed);
		/// Loads the removed bundle again, from its backup if it
		/// has one, and starts the bundles that have been stopped.

	void startBundles(const std::vector<Bundle::Ptr>& bundles);
		/// Starts the given bundles that removeBundle() has stopped,
		/// in the reverse order. A bundle that fails to start is
		/// logged.

private:

	std::vector<std::string> _paths;
	BundleLoader&            _loader;
	BundleFilter::Ptr        _pFilter;
	Poco::Logger&            _logger;
	Poco::Mutex              _installMutex;
		/// Protects the file operations of installs, the names of the
		/// installs in progress and _tempBundleSerial.
	std::set<std::string>    _installsInProgress;
	unsigned                 _tempBundleSerial = 0;
};


//
// inlines
//
inline const std::vector<std::string>& BundleRepository::paths() const
{
	return _paths;
}


} // namespace Poco::OSP


#endif // OSP_BundleRepository_INCLUDED
