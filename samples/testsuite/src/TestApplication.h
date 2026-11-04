//
// TestApplication.h
//
// Definition of the TestApplication class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestApplication_INCLUDED
#define TestApplication_INCLUDED


#include "Poco/Util/Application.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/TemporaryFile.h"
#include "Poco/AutoPtr.h"
#include <set>
#include <string>


class TestApplication: public Poco::Util::Application
	/// Runs bundles of the build in the test process, as BundleServer
	/// does, with the data directory in a temporary directory that is
	/// removed when the application is destroyed, and a code cache that
	/// all TestApplications of the process share, unless the application
	/// is given a private one.
	///
	/// Only one TestApplication can exist at a time.
{
public:
	using Ptr = Poco::AutoPtr<TestApplication>;

	TestApplication() = delete;

	explicit TestApplication(const std::set<std::string>& bundles);
		/// Creates the application for the bundles with the given
		/// symbolic names, found in the bundle directory of the build.
		/// Settings for the bundles can be added to config() before start().

	TestApplication(const TestApplication&) = delete;
	TestApplication& operator = (const TestApplication&) = delete;

	void start();
		/// Loads, resolves and starts the bundles.

	void stop();
		/// Stops and unloads the bundles.

	void dispatch(Poco::Net::HTTPServerRequest& request);
		/// Lets the WebServerDispatcher handle the request, as the
		/// web server does, writing to request.response().

	[[nodiscard]] Poco::OSP::BundleLoader& bundleLoader();
		/// Returns the BundleLoader of the started application.

	[[nodiscard]] Poco::OSP::ServiceRegistry& serviceRegistry();
		/// Returns the ServiceRegistry of the started application.

	[[nodiscard]] static std::string bundleFile(const std::string& bundle);
		/// Returns the path of the file of the bundle with the given
		/// symbolic name in the bundle directory of the build.

	[[nodiscard]] static std::string privateCodeCachePath();
		/// Returns a new code cache directory for a TestApplication, to be set
		/// as osp.codeCache in config() before start(). An application that
		/// uninstalls a bundle needs it: uninstalling removes the library of the
		/// bundle, which Windows does not do while an earlier application of the
		/// process has it loaded.

	static void copyBundles(const std::set<std::string>& bundles, const std::string& directory);
		/// Copies the bundles with the given symbolic names from the
		/// bundle directory of the build into the given directory,
		/// which is created if needed.

protected:
	~TestApplication() override;

private:
	Poco::TemporaryFile _directory;
	Poco::OSP::OSPSubsystem* _pOSP;
};


#endif // TestApplication_INCLUDED
