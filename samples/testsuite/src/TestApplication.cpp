//
// TestApplication.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestApplication.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Glob.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <set>
#include <string>


using namespace std::string_literals;


namespace
{
	Poco::Path processDirectory()
		/// Returns the temporary directory of the process, which holds the code
		/// caches. OSP does not unload the libraries of the bundles when an
		/// application ends, and Windows does not remove a library that is still
		/// loaded, so a directory per application would leave one behind for each.
	{
		static const Poco::TemporaryFile directory;
		return Poco::Path::forDirectory(directory.path());
	}
}


TestApplication::TestApplication(const std::set<std::string>& bundles):
	_pOSP(new Poco::OSP::OSPSubsystem)
{
	// The bundles are copied, so that a test can uninstall a bundle,
	// which removes its file.
	const Poco::Path directory(Poco::Path::forDirectory(_directory.path()));
	const Poco::Path repository(directory, "bundles"s);
	copyBundles(bundles, repository.toString());
	// Shared, as loading a library again from another path would load a second copy of it.
	config().setString("osp.codeCache"s, Poco::Path(processDirectory()).pushDirectory("codeCache"s).toString());
	config().setString("osp.data"s, Poco::Path(directory, "data"s).toString());
	config().setString("osp.bundleRepository"s, repository.toString());
	config().setString("logging.loggers.root.level"s, "warning"s);
	addSubsystem(_pOSP);
	init(ArgVec{"TestApplication"s});
}


TestApplication::~TestApplication()
{
	try
	{
		uninitialize();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


std::string TestApplication::privateCodeCachePath()
{
	return Poco::TemporaryFile::tempName(processDirectory().toString());
}


void TestApplication::start()
{
	initialize(*this);
}


void TestApplication::stop()
{
	uninitialize();
}


void TestApplication::dispatch(Poco::Net::HTTPServerRequest& request)
{
	Poco::OSP::ServiceRef::Ptr pDispatcherRef = serviceRegistry().findByName(Poco::OSP::Web::WebServerDispatcher::SERVICE_NAME);
	if (pDispatcherRef == nullptr)
	{
		throw Poco::NotFoundException("Service"s, Poco::OSP::Web::WebServerDispatcher::SERVICE_NAME);
	}
	pDispatcherRef->castedInstance<Poco::OSP::Web::WebServerDispatcher>()->handleRequest(request, request.response(), false);
}


Poco::OSP::BundleLoader& TestApplication::bundleLoader()
{
	return _pOSP->bundleLoader();
}


Poco::OSP::ServiceRegistry& TestApplication::serviceRegistry()
{
	return _pOSP->serviceRegistry();
}


std::string TestApplication::bundleFile(const std::string& bundle)
{
	std::set<std::string> files;
	Poco::Glob::glob(Poco::Path(Poco::Path::forDirectory(OSP_SAMPLES_BUNDLE_DIR), bundle + "_*.bndl"s), files);
	if (files.size() != 1)
	{
		throw Poco::NotFoundException("Exactly one file of the bundle in "s + OSP_SAMPLES_BUNDLE_DIR, bundle);
	}
	return *files.begin();
}


void TestApplication::copyBundles(const std::set<std::string>& bundles, const std::string& directory)
{
	Poco::File(directory).createDirectories();
	for (const auto& bundle: bundles)
	{
		Poco::File(bundleFile(bundle)).copyTo(directory);
	}
}
