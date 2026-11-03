//
// BundleRepository.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleRepository
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Glob.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/StreamCopier.h"
#include "Poco/Mutex.h"
#include "Poco/NumberFormatter.h"
#include "Poco/String.h"
#include "Poco/Exception.h"
#include "Poco/Bugcheck.h"
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>


using namespace std::string_literals;


using Poco::StringTokenizer;
using Poco::Path;
using Poco::File;
using Poco::DirectoryIterator;
using Poco::StreamCopier;
using Poco::Logger;
using Poco::NumberFormatter;


namespace Poco::OSP {


namespace
{
	[[nodiscard]] std::string currentExceptionText()
		/// Returns the text of the exception that is being handled.
	{
		try
		{
			throw;
		}
		catch (Poco::Exception& exc)
		{
			return exc.displayText();
		}
		catch (std::exception& exc)
		{
			return exc.what();
		}
		catch (...)
		{
			return "unknown exception";
		}
	}


	class InstallClaim
		/// Holds the symbolic names that an install works on in the set
		/// of names of the installs in progress, and removes them from
		/// the set when it is destroyed.
	{
	public:
		InstallClaim() = delete;
		InstallClaim(std::set<std::string>& names, Poco::Mutex& mutex):
			_names(names),
			_mutex(mutex)
		{
		}

		~InstallClaim()
		{
			try
			{
				Poco::Mutex::ScopedLock lock(_mutex);
				for (const auto& name: _claimed)
				{
					_names.erase(name);
				}
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		InstallClaim(const InstallClaim&) = delete;
		InstallClaim(InstallClaim&&) = delete;
		InstallClaim& operator = (const InstallClaim&) = delete;
		InstallClaim& operator = (InstallClaim&&) = delete;

		void claim(const std::string& name)
			/// Adds the name to the set. Throws a BundleInstallException
			/// if another install works on it. Requires the mutex to be held.
		{
			if (_names.find(name) != _names.end())
				throw BundleInstallException("Another install or replacement of the bundle is in progress", name);
			_names.insert(name);
			_claimed.push_back(name);
		}

	private:
		std::set<std::string>& _names;
		Poco::Mutex& _mutex;
		std::vector<std::string> _claimed;
	};
}


const std::string BundleRepository::TEMP_BUNDLE_EXTENSION("tmp");


BundleRepository::BundleRepository(const std::string& path, BundleLoader& loader, BundleFilter::Ptr pFilter):
	_loader(loader),
	_pFilter(pFilter),
	_logger(Logger::get("osp.core.BundleRepository"s))
{
#ifdef POCO_OS_FAMILY_UNIX
	std::string pathSep(":;");
#else
	std::string pathSep(";");
#endif
	StringTokenizer tokenizer(path, pathSep, StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);
	_paths.insert(_paths.end(), tokenizer.begin(), tokenizer.end());

	if (_paths.empty())
		throw Poco::InvalidArgumentException("At least one path must be supplied to a BundleRepository");
}


BundleRepository::BundleRepository(const std::vector<std::string>& paths, BundleLoader& loader, BundleFilter::Ptr pFilter):
	_paths(paths),
	_loader(loader),
	_pFilter(pFilter),
	_logger(Logger::get("osp.core.BundleRepository"s))
{
	if (_paths.empty())
		throw Poco::InvalidArgumentException("At least one path must be supplied to a BundleRepository");
}


BundleRepository::~BundleRepository() = default;


void BundleRepository::loadBundles()
{
	BundleMap bundles;
	for (std::vector<std::string>::const_iterator it = _paths.begin(); it != _paths.end(); ++it)
	{
		try
		{
			loadBundles(*it, bundles);
		}
		catch (Poco::Exception& exc)
		{
			std::string msg("Failed to load bundles from ");
			msg += "'";
			msg += *it;
			msg += "': ";
			msg += exc.displayText();
			_logger.error(msg);
		}
	}
	for (BundleMap::iterator it = bundles.begin(); it != bundles.end(); ++it)
	{
		_loader.loadBundle(it->second);
		if (isInRepository(it->second->path(), _paths[0]))
		{
			_loader.setDeleteOnUninstall(it->second, true);
		}
		if (_logger.information())
		{
			_logger.information("Loaded bundle %s/%s."s,
				it->second->symbolicName(),
				it->second->version().toString());
		}
	}
}


void BundleRepository::loadBundles(const std::string& path, BundleMap& bundles)
{
	std::set<std::string> paths;
#if defined(POCO_OS_FAMILY_WINDOWS)
	int options = Poco::Glob::GLOB_CASELESS;
	Poco::Glob::glob(path, paths, options);
#elif POCO_OS == POCO_OS_ANDROID
	int options = Poco::Glob::GLOB_DEFAULT;
	Path pathPattern = Path(Path::expand((path + "/*")), Path::PATH_GUESS);
	Path basePath = Path(Path::expand((path + "/")), Path::PATH_GUESS);
	Poco::Glob::glob(pathPattern, basePath, paths, options);
#else
	int options = Poco::Glob::GLOB_DEFAULT;
	Poco::Glob::glob(path, paths, options);
#endif

	for (std::set<std::string>::const_iterator git(paths.begin()), end(paths.end()); git != end; ++git)
	{
		Poco::Path p(*git);
		Poco::File f(*git);
		if (f.exists())
		{
			if (f.isFile() || (f.isDirectory() && Poco::icompare(p.getExtension(), "bndl") == 0))
			{
				try
				{
					if (_logger.debug())
					{
						_logger.debug("Found bundle %s."s, *git);
					}
					loadBundle(*git, bundles);
				}
				catch (Poco::Exception& exc)
				{
					std::string msg("Failed to load bundle from ");
					msg += "'";
					msg += *git;
					msg += "': ";
					msg += exc.displayText();
					_logger.error(msg);
				}
			}
			else
			{
				// in the order of their names, so that of two bundles of the same
				// version, the same one is loaded on every file system
				std::set<File> entries;
				DirectoryIterator it(*git);
				DirectoryIterator end;
				for (; it != end; ++it)
				{
					entries.insert(*it);
				}
				for (const File& entry: entries)
				{
					try
					{
						// a temporary file is left by an install that did not complete
						bool isBundle = !entry.isHidden() && Poco::icompare(Path(entry.path()).getExtension(), TEMP_BUNDLE_EXTENSION) != 0;
						if (entry.isDirectory())
						{
							Poco::Path metaPath(entry.path());
							metaPath.makeDirectory();
							metaPath.setFileName("META-INF");
							Poco::File metaDir(metaPath.toString());
							if (!metaDir.exists() || !metaDir.isDirectory())
							{
								isBundle = false;
							}
						}
						if (isBundle)
						{
							if (_logger.debug())
							{
								_logger.debug("Found bundle %s."s, entry.path());
							}
							loadBundle(entry.path(), bundles);
						}
					}
					catch (Poco::Exception& exc)
					{
						std::string msg("Failed to load bundle from ");
						msg += "'";
						msg += entry.path();
						msg += "': ";
						msg += exc.displayText();
						_logger.error(msg);
					}
				}
			}
		}
	}
}


void BundleRepository::loadBundle(const std::string& path, BundleMap& bundles)
{
	Bundle::Ptr pBundle(_loader.createBundle(path));

	if (_pFilter != nullptr && !_pFilter->accept(pBundle))
	{
		std::string msg("Bundle rejected by filter: ");
		msg += pBundle->symbolicName();
		msg += "/";
		msg += pBundle->version().toString();
		_logger.information(msg);
		return;
	}

	const std::string& id = pBundle->symbolicName();
	BundleMap::iterator it = bundles.find(id);
	if (it != bundles.end())
	{
		std::string msg("Conflicting versions detected for bundle ");
		msg += id;
		msg += "; ";
		if (it->second->version() <= pBundle->version())
		{
			msg += "chosing version ";
			msg += pBundle->version().toString();
			msg += " from '";
			msg += pBundle->path();
			msg += "' over version ";
			msg += it->second->version().toString();
			msg += " from '";
			msg += it->second->path();
			msg += "'";
			it->second = pBundle;
		}
		else
		{
			msg += "chosing version ";
			msg += it->second->version().toString();
			msg += " from '";
			msg += it->second->path();
			msg += "' over version ";
			msg += pBundle->version().toString();
			msg += " from '";
			msg += pBundle->path();
			msg += "'";
		}
		_logger.warning(msg);
	}
	else
	{
		bundles[id] = pBundle;
	}
}


Bundle::Ptr BundleRepository::installBundle(std::istream& istr)
{
	poco_assert (!_paths.empty());

	return installBundleImpl(istr, _paths[0], std::string());
}


Bundle::Ptr BundleRepository::installBundle(const std::string& repositoryPath, std::istream& istr)
{
	return installBundleImpl(istr, repositoryPath, std::string());
}


Bundle::Ptr BundleRepository::installBundle(std::istream& istr, const std::string& replaceBundle)
{
	poco_assert (!_paths.empty());

	return installBundleImpl(istr, _paths[0], replaceBundle);
}


Bundle::Ptr BundleRepository::installBundle(const std::string& repositoryPath, std::istream& istr, const std::string& replaceBundle)
{
	return installBundleImpl(istr, repositoryPath, replaceBundle);
}


Bundle::Ptr BundleRepository::installBundleImpl(std::istream& istr, const std::string& path, const std::string& replaceBundle)
{
	// _installMutex is held for the file operations and the set of installs
	// in progress only: stopping and starting bundles and firing events runs
	// bundle code, which may install a bundle itself.
	Path p(path);
	p.makeDirectory();
	Poco::FileOutputStream ostr;
	{
		Poco::Mutex::ScopedLock lock(_installMutex);
		p.setFileName(tempBundleName(path));
		ostr.open(p.toString());
	}

	File f(p);
	if (ostr.good())
	{
		InstallClaim claim(_installsInProgress, _installMutex);
		std::string symbolicName;
		RemovedBundle removed;
		try
		{
			StreamCopier::copyStream(istr, ostr);
			ostr.close();

			Bundle::Ptr pBundle(_loader.createBundle(f.path()));
			if (!_pFilter.isNull() && !_pFilter->accept(pBundle))
			{
				throw BundleInstallException("Bundle rejected by filter", pBundle->symbolicName());
			}
			symbolicName = pBundle->symbolicName();
			std::string newName(symbolicName);
			newName += "_";
			newName += pBundle->version().toString();
			newName += ".bndl";
			pBundle = nullptr;

			Path np(p);
			np.setFileName(newName);
			{
				// Two installs of the same bundle would both pass the checks.
				Poco::Mutex::ScopedLock lock(_installMutex);
				claim.claim(symbolicName);
				if (!replaceBundle.empty() && replaceBundle != symbolicName)
				{
					claim.claim(replaceBundle);
				}

				if (replaceBundle != symbolicName)
				{
					Bundle::Ptr pExistingBundle = _loader.findBundle(symbolicName);
					if (pExistingBundle != nullptr)
					{
						throw Poco::OSP::BundleInstallException("A bundle with the same symbolic name is already installed", symbolicName);
					}
				}

				if (File(np).exists() && !isReplacedBundle(np.toString(), replaceBundle))
				{
					throw BundleInstallException("A file or directory of the bundle's name, which is not the replaced bundle, exists", np.toString());
				}
			}

			if (!replaceBundle.empty())
			{
				removeBundle(replaceBundle, path, removed);
			}

			{
				Poco::Mutex::ScopedLock lock(_installMutex);
				f.renameTo(np.toString());
			}

			pBundle = _loader.loadBundle(np.toString());
			if (isInRepository(pBundle->path(), _paths[0]))
			{
				_loader.setDeleteOnUninstall(pBundle, true);
			}
			if (_logger.information())
			{
				_logger.information("Installed bundle %s/%s to '%s'."s,
					pBundle->symbolicName(),
					pBundle->version().toString(),
					f.path());
			}
			BundleEvent installedEvent(pBundle, BundleEvent::EV_BUNDLE_INSTALLED);
			_loader.events().bundleInstalled(this, installedEvent);

			if (!removed.backupPath.empty())
			{
				try
				{
					File(removed.backupPath).remove(true);
				}
				catch (Poco::Exception& exc)
				{
					_logger.warning("Failed to delete '%s', the backup of the replaced bundle %s: %s"s,
						removed.backupPath, replaceBundle, exc.displayText());
				}
			}
			startBundles(removed.stoppedBundles);
			return pBundle;
		}
		catch (...)
		{
			try
			{
				// loadBundle() fails after loading the bundle if a bundleLoaded delegate throws.
				Bundle::Ptr pLoadedBundle = _loader.findBundle(symbolicName);
				if (!pLoadedBundle.isNull() && pLoadedBundle->path() == f.path())
				{
					_loader.unloadBundle(pLoadedBundle);
				}
			}
			catch (...)
			{
				_logger.error("Failed to unload bundle %s, whose installation has failed: %s"s, symbolicName, currentExceptionText());
			}
			try
			{
				f.remove();
			}
			catch (...)
			{
			}
			restoreBundle(removed);
			throw;
		}
	}
	else throw Poco::CreateFileException(p.toString());
}


bool BundleRepository::isInRepository(const std::string& bundlePath, const std::string& repositoryPath)
{
	Path bundleParent(Path(bundlePath).parent());
	bundleParent.makeAbsolute();
	Path repository(repositoryPath);
	repository.makeDirectory();
	repository.makeAbsolute();
	return bundleParent.toString() == repository.toString();
}


bool BundleRepository::isReplacedBundle(const std::string& path, const std::string& replaceBundle) const
{
	Bundle::Ptr pBundle = _loader.findBundle(replaceBundle);
	if (pBundle.isNull()) return false;

	Path bundlePath(pBundle->path());
	bundlePath.makeFile();
	bundlePath.makeAbsolute();
	Path otherPath(path);
	otherPath.makeFile();
	otherPath.makeAbsolute();
	return bundlePath.toString() == otherPath.toString();
}


std::string BundleRepository::tempBundleName(const std::string& directory)
{
	// A temporary file or a backup left by an install that did not complete,
	// in this or in an earlier process, keeps its name.
	Path p(directory);
	p.makeDirectory();
	for (;;)
	{
		std::string name("tmp.bundle-");
		name += NumberFormatter::format(_tempBundleSerial++);
		name += ".";
		name += TEMP_BUNDLE_EXTENSION;
		p.setFileName(name);
		if (!File(p).exists()) return name;
	}
}


void BundleRepository::removeBundle(const std::string& symbolicName, const std::string& repositoryPath, RemovedBundle& removed)
{
	Bundle::Ptr pBundle = _loader.findBundle(symbolicName);
	if (pBundle != nullptr)
	{
		std::vector<Bundle::Ptr> requiringBundles;
		_loader.listRequiringBundles(pBundle, requiringBundles);
		for (auto& pRequiringBundle: requiringBundles)
		{
			if (pRequiringBundle->isActive())
			{
				pRequiringBundle->stop();
				removed.stoppedBundles.push_back(pRequiringBundle);
			}
		}
		if (pBundle->state() == Bundle::BUNDLE_ACTIVE)
		{
			pBundle->stop();
		}
		if (pBundle->state() == Bundle::BUNDLE_RESOLVED
		 || pBundle->state() == Bundle::BUNDLE_INSTALLED)
		{
			// A bundle in the primary path, whose file or directory uninstall()
			// deletes, and a bundle in repositoryPath, whose file the new bundle
			// may take the place of, are kept as a backup until the new bundle
			// has been loaded.
			const bool deleteOnUninstall = isInRepository(pBundle->path(), _paths[0]);
			const bool backUp = deleteOnUninstall || isInRepository(pBundle->path(), repositoryPath);
			if (backUp) _loader.setDeleteOnUninstall(pBundle, false);
			try
			{
				pBundle->uninstall();
			}
			catch (...)
			{
				if (deleteOnUninstall) _loader.setDeleteOnUninstall(pBundle, true);
				throw;
			}
			removed.path = pBundle->path();
			removed.deleteOnUninstall = deleteOnUninstall;
			if (backUp)
			{
				Poco::Mutex::ScopedLock lock(_installMutex);
				Path backupPath(Path(removed.path).parent());
				backupPath.setFileName(tempBundleName(backupPath.toString()));
				File(removed.path).renameTo(backupPath.toString());
				removed.backupPath = backupPath.toString();
			}
		}
		else throw BundleStateException("Cannot remove bundle in state" , pBundle->stateString());
	}
}


void BundleRepository::restoreBundle(const RemovedBundle& removed)
{
	bool restored = !removed.path.empty();
	if (restored && !removed.backupPath.empty())
	{
		try
		{
			Poco::Mutex::ScopedLock lock(_installMutex);
			File(removed.backupPath).renameTo(File(removed.path).path());
		}
		catch (...)
		{
			_logger.error("Failed to move the replaced bundle back from '%s' to '%s'. The bundle is not available until it is moved back: %s"s,
				removed.backupPath, removed.path, currentExceptionText());
			restored = false;
		}
	}
	if (restored)
	{
		try
		{
			Bundle::Ptr pBundle = _loader.loadBundle(removed.path);
			if (removed.deleteOnUninstall)
			{
				_loader.setDeleteOnUninstall(pBundle, true);
			}
		}
		catch (...)
		{
			_logger.error("Failed to load the replaced bundle from '%s' again. The bundle is not available: %s"s,
				removed.path, currentExceptionText());
		}
	}
	startBundles(removed.stoppedBundles);
}


void BundleRepository::startBundles(const std::vector<Bundle::Ptr>& bundles)
{
	// In the reverse order of stopping: a bundle after the bundles it requires.
	for (std::vector<Bundle::Ptr>::const_reverse_iterator it = bundles.rbegin(); it != bundles.rend(); ++it)
	{
		Bundle::Ptr pBundle = *it;
		if (pBundle->isActive()) continue;
		try
		{
			pBundle->start();
		}
		catch (...)
		{
			_logger.error("Failed to start bundle %s, which has been stopped to replace a bundle it requires: %s"s,
				pBundle->symbolicName(), currentExceptionText());
		}
	}
}


} // namespace Poco::OSP
