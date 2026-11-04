//
// BundleCreator.cpp
//
// The BundleCreator utility creates a bundle from a bundle specification file.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/XMLConfiguration.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/Version.h"
#include "Poco/Zip/Compress.h"
#include "Poco/Zip/ZipCommon.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Environment.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/Glob.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/FileStream.h"
#include "Poco/Thread.h"
#include "Poco/Random.h"
#include "Poco/Process.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Ascii.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Error.h"
#include "ManifestInfo.h"
#include <iostream>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <set>
#include <string>
#ifndef POCO_OS_FAMILY_WINDOWS
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif


using Poco::Util::Application;
using Poco::Util::XMLConfiguration;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::AbstractConfiguration;
using Poco::Util::OptionCallback;
using Poco::OSP::BundleManifest;
using Poco::OSP::Version;
using Poco::Zip::Compress;
using Poco::File;
using Poco::Path;
using Poco::DirectoryIterator;
using Poco::FileOutputStream;
using Poco::FileInputStream;
using namespace std::string_literals;


class FileLock
{
public:
	FileLock(const std::string& path):
		_file(path + ".lock")
	{
		acquire();
	}

	~FileLock()
	{
		try
		{
			release();
		}
		catch (...)
		{
		}
	}

protected:
	void acquire()
	{
		Poco::Random rnd;
		rnd.seed();
		int attempts = 0;
		bool haveLock = createFile();
		while (!haveLock)
		{
			if (++attempts > 100) throw Poco::FileException("Cannot acquire lock for bundle directory"s, _file.path());

			Poco::Thread::sleep(500 + rnd.next(2000));
			haveLock = createFile();
		}
	}

	void release()
	{
		_file.remove();
	}

	bool createFile()
	{
		try
		{
			return _file.createFile();
		}
		catch (Poco::Exception&)
		{
			return false;
		}
	}

private:
	FileLock();
	FileLock(const FileLock&);
	FileLock& operator = (const FileLock&);

	File _file;
};


class BundleCreatorApplication: public Application
{
public:
	BundleCreatorApplication():
		_osName(Poco::Environment::osName()),
		_osArch(Poco::Environment::osArchitecture()),
		_showHelp(false),
		_outputDir(Path::current()),
		_keep(false),
		_noDeflate(false),
		_noFile(false),
		_symlinks(false),
		_defaultVersion("1.0.0"s),
		_strip(Poco::Environment::get("BUNDLE_STRIP"s, ""s))
	{
		makeValidFileName(_osName);
		makeValidFileName(_osArch);
	}

protected:
	void initialize(Application& self)
	{
		loadConfiguration(); // load default configuration files, if present
		Application::initialize(self);
	}

	void defineOptions(OptionSet& options)
	{
		Application::defineOptions(options);

		options.addOption(
			Option("help"s, "h"s, "Display help information on command line arguments."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleHelp)));

		options.addOption(
			Option("output-dir"s, "o"s, "Specify the directory where the bundle is saved."s)
				.required(false)
				.repeatable(false)
				.argument("<file>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleOutput)));

		options.addOption(
			Option("keep-bundle-dir"s, "k"s, "Keep intermediary bundle directory."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleKeep)));

		options.addOption(
			Option("bundle-dir-only"s, "d"s, "Create bundle directory only, no bundle (.bndl) file (implies --keep-bundle-dir)."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleBundleDir)));

		options.addOption(
			Option("osname"s, "n"s, "Specify default target operating system name (e.g., \"Linux\")."s)
				.required(false)
				.repeatable(false)
				.argument("<osname>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleOSName)));

		options.addOption(
			Option("osarch"s, "a"s, "Specify default target operating system architecture (e.g., \"x86_64\" or \"armv7l\")."s)
				.required(false)
				.repeatable(false)
				.argument("<osarch>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleOSArch)));

		options.addOption(
			Option("no-deflate"s, "N"s, "Do not compress (deflate) files in bundle file. If a comma-separated list of extensions "
				"is specified, only files with these extensions are stored uncompressed."s)
				.required(false)
				.repeatable(false)
				.argument("<extensions>"s, false)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleNoDeflate)));

		options.addOption(
			Option("version"s, "v"s, "Specify default bundle version, e.g. \"1.0.0\", to be used "
				"if the bundle specification file does not have one."s)
				.required(false)
				.repeatable(false)
				.argument("<version>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleVersion)));

		options.addOption(
			Option("define"s, "D"s,
				"Define a configuration property. A configuration property "
				"defined with this option can be referenced in the bundle specification "
				"file, using the following syntax: ${<name>}. "
				"Can be specified multiple times."s)
				.required(false)
				.repeatable(true)
				.argument("<name>=<value>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleDefine)));

		options.addOption(
			Option("code"s, "c"s,
				"Specify executable files to include in the bundle's bin directory. "
				"If specified, overrides the <code> element in the bundle specification file. "
				"Value is a comma- or semicolon-separated list of file names or glob expressions. "
				"Can be specified multiple times."s)
				.required(false)
				.repeatable(true)
				.argument("<glob>{;<glob>}"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleCode)));

		options.addOption(
			Option("strip"s, "S"s,
				"Specify the command that is used to strip debugging symbols from shared libraries "
				"and executables included in the bundle's bin directory. "
				"If specified, the specified strip command will be invoked for each file included "
				"in the bundle's bin directory."s)
				.required(false)
				.repeatable(false)
				.argument("<command>"s)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleStrip)));

		options.addOption(
			Option("symlinks"s, "y"s,
				"Preserve symbolic links in the bundle. When enabled, symlinks "
				"in <files> are stored as symlinks rather than being resolved. "
				"Uses the system zip command to create the bundle file. "
				"Not supported on Windows."s)
				.required(false)
				.repeatable(false)
				.callback(OptionCallback<BundleCreatorApplication>(this, &BundleCreatorApplication::handleSymlinks)));
	}

	void handleHelp(const std::string& name, const std::string& value)
	{
		_showHelp = true;
		stopOptionsProcessing();
	}

	void handleOutput(const std::string& name, const std::string& value)
	{
		_outputDir = value;
	}

	void handleKeep(const std::string& name, const std::string& value)
	{
		_keep = true;
	}

	void handleBundleDir(const std::string& name, const std::string& value)
	{
		_keep = true;
		_noFile = true;
	}

	void handleOSName(const std::string& name, const std::string& value)
	{
		_osName = value;
		makeValidFileName(_osName);
	}

	void handleOSArch(const std::string& name, const std::string& value)
	{
		_osArch = value;
		makeValidFileName(_osArch);
	}

	void handleNoDeflate(const std::string& name, const std::string& value)
	{
		if (value.empty())
		{
			_noDeflate = true;
		}
		else
		{
			Poco::StringTokenizer tok(value, ",;"s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
			_storeExtensions.insert(tok.begin(), tok.end());
		}
	}

	void handleVersion(const std::string& name, const std::string& value)
	{
		_defaultVersion = value;
	}

	void handleDefine(const std::string& name, const std::string& value)
	{
		defineProperty(value);
	}

	void handleCode(const std::string& name, const std::string& value)
	{
		if (_code.empty())
		{
			_code = value;
		}
		else
		{
			_code += ';';
			_code += value;
		}
	}

	void handleStrip(const std::string& name, const std::string& value)
	{
		_strip = value;
	}

	void handleSymlinks(const std::string& name, const std::string& value)
	{
#ifdef POCO_OS_FAMILY_WINDOWS
		throw Poco::NotImplementedException("The --symlinks option is not supported on Windows"s);
#else
		_symlinks = true;
#endif
	}

	void defineProperty(const std::string& def)
	{
		std::string name;
		std::string value;
		std::string::size_type pos = def.find('=');
		if (pos != std::string::npos)
		{
			name.assign(def, 0, pos);
			value.assign(def, pos + 1, def.length() - pos);
		}
		else name = def;
		config().setString(name, value);
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("[<option> ...] <file> ..."s);
		helpFormatter.setHeader(
			"\n"
			"The Applied Informatics OSP Bundle Creator Utility.\n"
			"Copyright (c) 2007-2026 by Applied Informatics Software Engineering GmbH.\n"
			"All rights reserved.\n\n"
			"This program builds bundle files for use with the "
			"Open Service Platform. What goes into a bundle "
			"is specified in a bundle specification file, passed "
			"as command line argument.\n\n"
			"The following command line options are supported:"s
		);
		helpFormatter.setFooter(
			"For more information, please see the Open Service Platform "
			"documentation at <https://www.appinf.com/docs>."s
		);
		helpFormatter.setIndent(8);
		helpFormatter.format(std::cout);
	}

	int main(const std::vector<std::string>& args)
	{
		if (_showHelp || args.empty())
		{
			displayHelp();
		}
		else
		{
			config().setString("osName"s, _osName);
			config().setString("osArch"s, _osArch);
			if (!config().hasProperty("bin"s))
			{
				if (_osArch == "AMD64")
				{
					config().setString("bin"s, "bin64"s);
					config().setString("64"s, "64"s);
				}
				else
				{
					config().setString("bin"s, "bin"s);
					config().setString("64"s, ""s);
				}
			}

			for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it)
			{
				_ptrCfg = new XMLConfiguration(*it);
				handleConfig(*it);
			}
		}
		return Application::EXIT_OK;
	}

private:
	void handleConfig(const std::string& specPath)
	{
		File outputDir(_outputDir);
		outputDir.createDirectories();

		Path outputPath(_outputDir);
		outputPath.makeDirectory();
		ManifestInfo mi(loadManifest());
		checkFileName(mi.symbolicName(), "symbolic name", specPath);
		checkFileName(mi.version().toString(), "version", specPath);
		std::string bndlName(mi.symbolicName() + "_" + mi.version().toString());
		Path bndlPath(outputPath, bndlName);

		File bndlDir(bndlPath);
		FileLock lock(bndlDir.path());
		_destinations.clear();
		if (bndlDir.exists())
		{
			safeRemove(bndlDir);
			bndlDir.remove(true);
		}
		bndlDir.createDirectories();
		try
		{
			handleCode(bndlPath, mi);
			handleMeta(bndlPath, mi);
			handleOther(bndlPath, mi);
			if (!_noFile)
			{
				Path bndlFilePath(bndlPath);
				bndlFilePath.setFileName(bndlFilePath.getFileName() + ".bndl");
				// The bundle file is written under a temporary name and renamed when it is complete,
				// so that an existing bundle file is replaced, and never updated or left truncated.
				// The extension keeps zip from appending ".zip" to the name.
				const std::string tempFilePath(Poco::TemporaryFile::tempName(outputPath.toString()) + ".bndl");
				try
				{
					createBundleFile(bndlPath, tempFilePath);
					File(tempFilePath).renameTo(bndlFilePath.toString());
				}
				catch (...)
				{
					File tempFile(tempFilePath);
					if (tempFile.exists()) tempFile.remove();
					throw;
				}
				if (!_keep)
				{
					safeRemove(bndlDir);
					bndlDir.remove(true);
				}
			}
		}
		catch (...)
		{
			if (!_keep)
			{
				safeRemove(bndlDir);
				bndlDir.remove(true);
			}
			throw;
		}
	}

	void createBundleFile(const Path& bndlPath, const std::string& filePath)
	{
#ifndef POCO_OS_FAMILY_WINDOWS
		if (_symlinks)
		{
			// Poco::Zip::Compress does not support storing symlinks -
			// it follows them, which fails on broken links and loses
			// the link structure. Use system zip -y which stores
			// symlinks via the de-facto Unix external_attr convention.
			Poco::Process::Args args;
			args.push_back("-ryq"s);
			if (_noDeflate)
			{
				args.push_back("-0"s);
			}
			else if (!_storeExtensions.empty())
			{
				std::string suffixes;
				for (const auto& extension: _storeExtensions)
				{
					if (!suffixes.empty()) suffixes += ':';
					suffixes += '.';
					suffixes += extension;
				}
				args.push_back("-n"s);
				args.push_back(suffixes);
			}
			args.push_back(Path(filePath).makeAbsolute().toString());
			args.push_back("."s);
			Poco::ProcessHandle ph = Poco::Process::launch("zip"s, args, bndlPath.toString());
			int rc = ph.wait();
			if (rc != 0)
				throw Poco::RuntimeException("zip command failed with exit code "s + std::to_string(rc));
		}
		else
#endif
		{
			FileOutputStream out(filePath);
			Compress compr(out, true);
			if (!_storeExtensions.empty())
			{
				compr.setStoreExtensions(_storeExtensions);
			}
			compr.addRecursive(bndlPath, _noDeflate ? Poco::Zip::ZipCommon::CM_STORE : Poco::Zip::ZipCommon::CM_AUTO, Poco::Zip::ZipCommon::CL_MAXIMUM, true);
			compr.close();
			out.close();
		}
	}

	ManifestInfo loadManifest()
	{
		static const std::string PREFIX("manifest.");
		std::string name = getString(PREFIX + "name");
		std::string symbolicName = getString(PREFIX + "symbolicName");
		std::string vendor = getString(PREFIX + "vendor");
		std::string copyright = getString(PREFIX + "copyright", ""s);
		std::string activatorClass = getString(PREFIX + "activator.class", ""s);
		std::string activatorLibrary = getString(PREFIX + "activator.library", ""s);
		bool lazyStart = getBool(PREFIX + "lazyStart", false);
		bool sealed = getBool(PREFIX + "sealed", false);
		bool preventUninstall = getBool(PREFIX + "preventUninstall", false);
		std::string runLevel = getString(PREFIX + "runLevel", BundleManifest::DEFAULT_RUNLEVEL);
		std::string extendsBundle = getString(PREFIX + "extends", ""s);
		std::string versionStr(getString(PREFIX + "version", ""s));
		if (versionStr.empty() || versionStr == "default" || versionStr == "auto")
		{
			versionStr = _defaultVersion;
		}
		Poco::OSP::Version version(versionStr);
		ManifestInfo::Dependencies requiredBundles;
		Poco::UInt32 idx = 0;
		std::string symName;
		std::string versionRange;
		do
		{
			std::string path(PREFIX+"dependency[");
			path.append(Poco::NumberFormatter::format(idx++));
			path.append("].");
			symName = path + "symbolicName";
			versionRange = path + "version";
			symName = getString(symName, ""s);
			versionRange = getString(versionRange, ""s);
			Poco::trimInPlace(symName);
			Poco::trimInPlace(versionRange);
			if (!symName.empty())
			{
				ManifestInfo::Dependency dep;
				dep.symbolicName = symName;
				if (!versionRange.empty())
				{
					dep.versions = versionRange;
				}
				requiredBundles.push_back(dep);
			}
		}
		while (!symName.empty());

		ManifestInfo::Dependencies requiredModules;
		idx = 0;
		do
		{
			std::string path(PREFIX+"module-dependency[");
			path.append(Poco::NumberFormatter::format(idx++));
			path.append("].");
			symName = path + "symbolicName";
			versionRange = path + "version";
			symName = getString(symName, ""s);
			versionRange = getString(versionRange, ""s);
			Poco::trimInPlace(symName);
			Poco::trimInPlace(versionRange);
			if (!symName.empty())
			{
				ManifestInfo::Dependency dep;
				dep.symbolicName = symName;
				if (!versionRange.empty())
				{
					dep.versions = versionRange;
				}
				requiredModules.push_back(dep);
			}
		}
		while (!symName.empty());

		ManifestInfo::ProvidedModules providedModules;
		idx = 0;
		do
		{
			std::string path(PREFIX+"module[");
			path.append(Poco::NumberFormatter::format(idx++));
			path.append("].");
			symName = path + "symbolicName";
			versionRange = path + "version";
			symName = getString(symName, ""s);
			versionRange = getString(versionRange, ""s);
			Poco::trimInPlace(symName);
			Poco::trimInPlace(versionRange);
			if (!symName.empty())
			{
				ManifestInfo::ProvidedModule mod;
				mod.symbolicName = symName;
				if (!versionRange.empty())
				{
					mod.version = versionRange;
				}
				providedModules.push_back(mod);
			}
		}
		while (!symName.empty());

		return ManifestInfo(name, symbolicName, version, vendor, copyright, activatorClass, activatorLibrary, requiredBundles, requiredModules, providedModules, lazyStart, sealed, preventUninstall, runLevel, extendsBundle);
	}

	void saveManifest(const ManifestInfo& info, std::ostream& out)
	{
		//Manifest-Version: 1.0
		//Bundle-Name: OSP Sample Bundle 9
		//Bundle-SymbolicName: com.appinf.osp.bundle9
		//Bundle-Version: 0.9.1
		//Bundle-Vendor: Applied Informatics
		//Bundle-Copyright: (c) 2007, Applied Informatics Software Engineering GmbH
		//Bundle-Activator: MyBundleActivator;library=TestBundle
		//Bundle-RunLevel: 200
		out << BundleManifest::MANIFEST_VERSION << ": " << BundleManifest::VERSION << std::endl;
		out << BundleManifest::BUNDLE_NAME << ": " << info.name() << std::endl;
		out << BundleManifest::BUNDLE_SYMBOLICNAME << ": " << info.symbolicName() << std::endl;
		out << BundleManifest::BUNDLE_VERSION << ": " << info.version().toString() << std::endl;
		out << BundleManifest::BUNDLE_VENDOR << ": " << info.vendor() << std::endl;
		if (!info.copyright().empty())
			out << BundleManifest::BUNDLE_COPYRIGHT << ": " << info.copyright() << std::endl;
		if (!info.activatorClass().empty() && !info.activatorLibrary().empty())
			out << BundleManifest::BUNDLE_ACTIVATOR << ": " << info.activatorClass() << ";library=" << info.activatorLibrary() << std::endl;
		if (!info.runLevel().empty())
			out << BundleManifest::BUNDLE_RUNLEVEL << ": " << info.runLevel() << std::endl;
		if (!info.extendsBundle().empty())
			out << BundleManifest::EXTENDS_BUNDLE << ": " << info.extendsBundle() << std::endl;
		out << BundleManifest::BUNDLE_LAZYSTART << ": " << (info.lazyStart()?"true":"false") << std::endl;
		if (info.sealed())
			out << BundleManifest::BUNDLE_SEALED << ": true" << std::endl;
		if (info.preventUninstall())
			out << BundleManifest::BUNDLE_PREVENTUNINSTALL << ": true" << std::endl;

		const ManifestInfo::Dependencies& deps = info.requiredBundles();
		//Require-Bundle: com.appinf.osp.bundle1;bundle-version=[1.0,2.0), com.appinf.osp.bundle2
		if (!deps.empty())
		{
			out << BundleManifest::REQUIRE_BUNDLE << ": ";
			std::string empty(BundleManifest::REQUIRE_BUNDLE.size()+2, ' ');
			ManifestInfo::Dependencies::const_iterator it = deps.begin();
			ManifestInfo::Dependencies::const_iterator itEnd = deps.end();
			for (; it != itEnd; ++it)
			{
				if (it != deps.begin())
				{
					out << ", \\" << std::endl << empty;
				}
				out << it->symbolicName;
				if (!it->versions.empty())
					out << ";bundle-version=" << it->versions;
			}
			out << std::endl;
		}

		const ManifestInfo::Dependencies& moduleDeps = info.requiredModules();
		//Require-Module: com.appinf.osp.module1;module-version=[1.0,2.0), com.appinf.osp.module2
		if (!moduleDeps.empty())
		{
			out << BundleManifest::REQUIRE_MODULE << ": ";
			std::string empty(BundleManifest::REQUIRE_MODULE.size()+2, ' ');
			ManifestInfo::Dependencies::const_iterator it = moduleDeps.begin();
			ManifestInfo::Dependencies::const_iterator itEnd = moduleDeps.end();
			for (; it != itEnd; ++it)
			{
				if (it != moduleDeps.begin())
				{
					out << ", \\" << std::endl << empty;
				}
				out << it->symbolicName;
				if (!it->versions.empty())
					out << ";module-version=" << it->versions;
			}
			out << std::endl;
		}

		const ManifestInfo::ProvidedModules& modules = info.providedModules();
		//Provide-Module: com.appinf.osp.module1;module-version=1.0.0
		if (!modules.empty())
		{
			out << BundleManifest::PROVIDE_MODULE << ": ";
			std::string empty(BundleManifest::PROVIDE_MODULE.size()+2, ' ');
			ManifestInfo::ProvidedModules::const_iterator it = modules.begin();
			ManifestInfo::ProvidedModules::const_iterator itEnd = modules.end();
			for (; it != itEnd; ++it)
			{
				if (it != modules.begin())
				{
					out << ", \\" << std::endl << empty;
				}
				out << it->symbolicName << ";module-version=" << it->version;
			}
			out << std::endl;
		}
	}

	void handleCode(const Path& root, const ManifestInfo& mi)
	{
		Path binDir(root, "bin"s);
		binDir.makeDirectory();

		if (!_code.empty())
		{
			std::string platform = _osName + "/" + _osArch;
			Path platformDir(binDir, Path(platform, Path::PATH_UNIX));
			copyCode(platformDir, _code, mi);
		}
		else
		{
			// now check for the code entries
			// <code [platform="Windows_NT/ia32"]>globExpression</code>
			Poco::UInt32 idx = 0;
			std::string globs;

			std::string path("code[");
			path.append(Poco::NumberFormatter::format(idx++));
			path.append("]");
			while (_ptrCfg->hasProperty(path))
			{
				globs = getString(path, ""s);
				path.append("[@platform]"s);
				std::string platform = getString(path, ""s);
				if (platform.empty() || platform.find("/") == std::string::npos)
				{
					platform = _osName + "/" + _osArch;
				}
				Path platformDir(binDir, Path(platform, Path::PATH_UNIX));
				copyCode(platformDir, globs, mi);

				path = "code[";
				path.append(Poco::NumberFormatter::format(idx++));
				path.append("]");
			}
		}
	}

	void copyCode(const Poco::Path& platformDir, const std::string& globs, const ManifestInfo& mi)
	{
		std::set<std::string> files;
		Poco::StringTokenizer incTokenizer(globs, ",;\n"s, Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
		for (Poco::StringTokenizer::Iterator it = incTokenizer.begin(); it != incTokenizer.end(); ++it)
		{
			Poco::Glob::glob(*it, files, Poco::Glob::GLOB_DOT_SPECIAL);
		}

		std::set<std::string>::const_iterator itF = files.begin();
		std::set<std::string>::const_iterator itFEnd = files.end();
		for (; itF != itFEnd; ++itF)
		{
			File aFile(*itF);
			bool exists = aFile.exists();
#ifndef POCO_OS_FAMILY_WINDOWS
			// A dangling symbolic link does not exist(); with --symlinks, it is copied as in <files>.
			struct stat linkStat{};
			if (!exists && _symlinks && ::lstat(aFile.path().c_str(), &linkStat) == 0) exists = S_ISLNK(linkStat.st_mode);
#endif
			if (exists)
			{
				File platformFile(platformDir);
				platformFile.createDirectories();
				copyFile(aFile, platformDir.toString());
			}
		}
		if (!files.empty() && !_strip.empty())
		{
			stripCode(platformDir);
		}
		if (files.empty() && incTokenizer.count() > 0)
		{
			std::cerr << Poco::format("Warning: Non-empty <code> element, but no files found for expression '%s' while building bundle %s."s, Poco::cat("; "s, incTokenizer.begin(), incTokenizer.end()), mi.symbolicName()) << std::endl;
		}
	}

	void stripCode(const Poco::Path& platformDir)
	{
		File platformFile(platformDir.toString());
		std::vector<Poco::File> files;
		platformFile.list(files);
		for (const auto& file: files)
		{
			if (file.isFile())
			{
				logger().debug("Stripping file: %s"s, file.path());
				Poco::Process::Args args;
				args.push_back(file.path());
				Poco::ProcessHandle ph = Poco::Process::launch(_strip, args);
				int rc = ph.wait();
				if (rc != 0)
				{
					std::cerr << "Warning: strip command for " << file.path() << " failed with exit code " << rc << std::endl;
				}
			}
		}
	}

	void handleMeta(const Path& root, const ManifestInfo& mi)
	{
		Path metaDir(root, "META-INF");
		metaDir.makeDirectory();
		File metaFile(metaDir);
		metaFile.createDirectories();
		Path manifest(metaDir, "manifest.mf"s);
		FileOutputStream out(manifest.toString());
		saveManifest(mi, out);
		out.close();
		// verify
		FileInputStream in(manifest.toString());
		BundleManifest::Ptr pManifest = new BundleManifest(in);
	}

	void handleOther(const Path& root, const ManifestInfo& mi)
	{
		if (_ptrCfg->hasProperty("files"s))
		{
			std::string globs = getString("files"s);
			std::set<std::string> files;
			Poco::StringTokenizer incTokenizer(globs, ",;\r"s, Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
			for (Poco::StringTokenizer::Iterator it = incTokenizer.begin(); it != incTokenizer.end(); ++it)
			{
				Poco::Glob::glob(*it, files, Poco::Glob::GLOB_DOT_SPECIAL);
			}

			for (std::set<std::string>::iterator itFiles = files.begin(); itFiles != files.end(); ++itFiles)
			{
				File aFile(*itFiles);
				copyFile(aFile, root.toString());
			}

			if (files.empty() && incTokenizer.count() > 0)
			{
				// Suppress the warning when any expression is a wildcard glob
				// (* ? [). An empty match for a glob is a legitimate case (e.g.
				// bundle/* with no resources), but an empty match for a plain
				// literal filename is almost always a typo and worth flagging.
				bool anyGlob = false;
				for (auto it = incTokenizer.begin(); it != incTokenizer.end(); ++it)
				{
					if (it->find_first_of("*?["s) != std::string::npos)
					{
						anyGlob = true;
						break;
					}
				}
				if (!anyGlob)
				{
					std::cerr << Poco::format("Warning: Non-empty <files> element, but no files found for expression '%s' while building bundle %s."s, Poco::cat("; "s, incTokenizer.begin(), incTokenizer.end()), mi.symbolicName()) << std::endl;
				}
			}
		}
	}

	static void checkFileName(const std::string& value, const std::string& what, const std::string& specPath)
	{
		// The symbolic name and the version become the name of the bundle directory and file,
		// which are removed and written.
		if (!Poco::OSP::BundleManifest::isValidFileName(value))
			throw Poco::DataFormatException(Poco::format("The bundle %s in %s cannot be used as a file name"s, what, specPath), value);
	}

	void makeValidFileName(std::string& name)
	{
		for (std::string::iterator it = name.begin(); it != name.end(); ++it)
		{
			if (!(Poco::Ascii::isAlphaNumeric(*it) || *it == '-')) *it = '_';
		}
	}

	void safeRemove(const File& p)
	{
		std::vector<File> entries;
		p.list(entries);
		std::vector<File>::iterator it = entries.begin();
		std::vector<File>::iterator itEnd = entries.end();
		for (; it != itEnd; ++it)
		{
#ifndef POCO_OS_FAMILY_WINDOWS
			// Poco::File methods (setWriteable, isDirectory, etc.) use stat()
			// which follows symlinks. On a broken symlink, stat() fails and
			// Poco::File throws. Must check isLink() (uses lstat()) first
			// and remove the link directly.
			if (it->isLink())
			{
				it->remove();
				continue;
			}
#endif
			it->setWriteable(true);
			if (it->isDirectory())
			{
				safeRemove(*it);
			}
			it->remove(true);
		}
	}

	void copyFile(const File& file, const std::string& destPath)
	{
		logger().debug("Copying %s to %s"s, file.path(), destPath);

		if (file.isHidden()) return;
#ifndef POCO_OS_FAMILY_WINDOWS
		// Copy symlinks using readlink/symlink directly, bypassing
		// Poco::File which would follow the link via stat() and throw
		// on broken symlinks. This preserves relative link targets.
		if (_symlinks && file.isLink())
		{
			Path src(file.path());
			Path dest(destPath);
			File destFile(destPath);
			if ((destFile.exists() && destFile.isDirectory()) || dest.isDirectory())
			{
				dest.makeDirectory();
				dest.setFileName(src.getFileName());
			}
			addDestination(dest.toString(), file.path());
			// Read the symlink target and recreate it
			std::string target(PATH_MAX, '\0');
			const ssize_t len = ::readlink(file.path().c_str(), target.data(), target.size() - 1);
			if (len < 0)
			{
				const int error = errno;
				throw Poco::FileException(Poco::format("Cannot read symbolic link: %s"s, Poco::Error::getMessage(error)), file.path());
			}
			if (static_cast<std::size_t>(len) == target.size() - 1)
				throw Poco::FileException("The target of the symbolic link is too long"s, file.path());
			target.resize(static_cast<std::size_t>(len));
			if (::symlink(target.c_str(), dest.toString().c_str()) != 0)
			{
				const int error = errno;
				throw Poco::FileException(Poco::format("Cannot create symbolic link to %s: %s"s, target, Poco::Error::getMessage(error)), dest.toString());
			}
			return;
		}
#endif

		Path src(file.path());
		Path dest(destPath);
		File destFile(destPath);
		if ((destFile.exists() && destFile.isDirectory()) || dest.isDirectory())
		{
			dest.makeDirectory();
			dest.setFileName(src.getFileName());
		}
		if (file.isDirectory())
		{
			copyDirectory(file, dest.toString());
		}
		else
		{
			addDestination(dest.toString(), file.path());
			file.copyTo(destPath);
		}
	}

	void addDestination(const std::string& destPath, const std::string& sourcePath)
	{
		// Files are copied by their name, so two source files can have the same destination.
		if (!_destinations.insert(destPath).second)
			throw Poco::FileExistsException(Poco::format("Two files are copied to %s; the second one is"s, destPath), sourcePath);
	}

	void copyDirectory(const File& file, const std::string& destPath)
	{
		File target(destPath);
		target.createDirectories();

		Path src(file.path());
		src.makeFile();
		DirectoryIterator it(src);
		DirectoryIterator end;
		for (; it != end; ++it)
		{
			copyFile(*it, destPath);
		}
	}

	std::string getString(const std::string& prop)
	{
		return config().expand(_ptrCfg->getString(prop));
	}

	std::string getString(const std::string& prop, const std::string& deflt)
	{
		return config().expand(_ptrCfg->getString(prop, deflt));
	}

	bool getBool(const std::string& prop, bool deflt)
	{
		return _ptrCfg->getBool(prop, deflt);
	}

private:
	std::string _osName;
	std::string _osArch;
	bool _showHelp;
	std::string _outputDir;
	bool _keep;
	bool _noDeflate;
	bool _noFile;
	bool _symlinks;
	Poco::AutoPtr<XMLConfiguration> _ptrCfg;
	std::set<std::string> _storeExtensions;
	std::string _defaultVersion;
	std::string _code;
	std::string _strip;
	std::set<std::string> _destinations;
};


POCO_APP_MAIN(BundleCreatorApplication)
