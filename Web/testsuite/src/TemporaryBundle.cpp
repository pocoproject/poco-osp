//
// TemporaryBundle.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TemporaryBundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/Zip/Compress.h"
#include "Poco/DateTime.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include <sstream>
#include <string>
#include <memory>


using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleFactory;
using Poco::OSP::BundleLoader;
using Poco::OSP::CodeCache;
using Poco::OSP::LanguageTag;
using Poco::Path;
using namespace std::string_literals;


namespace
{
	std::string manifest(const std::string& symbolicName)
	{
		return
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Web Test Bundle\n"
			"Bundle-SymbolicName: " + symbolicName + "\n"
			"Bundle-Version: 1.0.0\n"
			"Bundle-Vendor: Applied Informatics\n"
			"Bundle-Copyright: (c) 2026, Applied Informatics Software Engineering GmbH\n";
	}

	void addZipEntry(Poco::Zip::Compress& compress, const std::string& name, const std::string& content)
	{
		std::istringstream istr(content);
		compress.addFile(istr, Poco::DateTime(), Path(name, Path::PATH_UNIX));
	}
}


const std::string TemporaryBundle::HELLO_TXT("abcdefghijklmnopqrstuvwxyz");
const std::string TemporaryBundle::INDEX_HTML("<html><body>index</body></html>");


TemporaryBundle::TemporaryBundle(const std::string& properties)
{
	Path directory(_directory.path());
	directory.makeDirectory();

	Path dirBundlePath(directory);
	dirBundlePath.pushDirectory("osp.web.test.dir_1.0.0"s);
	writeFile(dirBundlePath, "META-INF/manifest.mf"s, manifest("osp.web.test.dir"s));
	writeFile(dirBundlePath, "bundle.properties"s, properties);
	writeFile(dirBundlePath, "www/hello.txt"s, HELLO_TXT);
	writeFile(dirBundlePath, "www/index.html"s, INDEX_HTML);

	Path unloadedBundlePath(directory);
	unloadedBundlePath.pushDirectory("osp.web.test.unloaded_1.0.0"s);
	writeFile(unloadedBundlePath, "META-INF/manifest.mf"s, manifest("osp.web.test.unloaded"s));
	writeFile(unloadedBundlePath, "bundle.properties"s, "web.static.resource = persist\nweb.static.strict = true\n"s);
	writeFile(unloadedBundlePath, "persist/file.txt"s, "bundle"s);

	const Path zipBundlePath(directory, "osp.web.test.zip_1.0.0.bndl"s);
	{
		Poco::FileOutputStream ostr(zipBundlePath.toString());
		Poco::Zip::Compress compress(ostr, true);
		addZipEntry(compress, "META-INF/manifest.mf"s, manifest("osp.web.test.zip"s));
		addZipEntry(compress, "www/hello.txt"s, HELLO_TXT);
		addZipEntry(compress, "www/index.html"s, INDEX_HTML);
		compress.close();
	}

	_pCodeCache = std::make_unique<CodeCache>(Path(directory, "codeCache/"s).toString());
	_pBundleContextFactory = new BundleContextFactory(_registry, _systemEvents, Path(directory, "persistency/"s));
	_pLoader = std::make_unique<BundleLoader>(*_pCodeCache, new BundleFactory(LanguageTag("en"s, "US"s)), _pBundleContextFactory);
	_pBundle = _pLoader->loadBundle(dirBundlePath.toString());
	_pZipBundle = _pLoader->loadBundle(zipBundlePath.toString());
	_pUnloadedBundle = _pLoader->createBundle(unloadedBundlePath.toString());
	_pContext = _pLoader->contextForBundle(_pBundle);
}


TemporaryBundle::~TemporaryBundle() = default;


void TemporaryBundle::writeFile(const Path& baseDir, const std::string& relativePath, const std::string& content)
{
	const Path path(baseDir, Path(relativePath, Path::PATH_UNIX));
	Poco::File(path.parent()).createDirectories();
	Poco::FileOutputStream ostr(path.toString());
	ostr << content;
}
