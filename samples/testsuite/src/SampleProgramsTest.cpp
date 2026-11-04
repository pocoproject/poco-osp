//
// SampleProgramsTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SampleProgramsTest.h"
#include "TestApplication.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Process.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/NullStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Event.h"
#if defined(POCO_OS_FAMILY_WINDOWS)
#include "Poco/Exception.h"
#endif
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>


using namespace std::string_literals;


namespace
{
	const std::string PREFERENCES_SAMPLE("com.appinf.osp.samples.preferences"s);
	constexpr std::chrono::milliseconds STARTUP_TIMEOUT{60000};
	// The option syntax of Poco::Util::Application on the platform
#if defined(POCO_OS_FAMILY_WINDOWS)
	const std::string CONFIG_FILE_OPTION("/config-file="s);
#else
	const std::string CONFIG_FILE_OPTION("--config-file="s);
#endif
}


SampleProgramsTest::SampleProgramsTest(const std::string& name): CppUnit::TestCase(name)
{
}


SampleProgramsTest::~SampleProgramsTest() = default;


void SampleProgramsTest::setUp()
{
	_pDirectory = std::make_unique<Poco::TemporaryFile>();
	_pDirectory->createDirectories();
}


void SampleProgramsTest::tearDown()
{
	_pDirectory.reset();
}


void SampleProgramsTest::testBundleContainerConfigFileTakesPrecedence()
{
	const std::vector<std::string> args{CONFIG_FILE_OPTION + writeConfiguration()};
	Poco::Pipe outPipe;
	Poco::ProcessHandle container = Poco::Process::launch(OSP_SAMPLES_BUNDLE_CONTAINER, args, nullptr, &outPipe, &outPipe);
	Poco::PipeInputStream output(outPipe);
	Poco::NullOutputStream discarded;
	Poco::StreamCopier::copyStream(output, discarded);

	assertEqual (0, container.wait());
	// The Preferences sample writes its preferences when the program ends.
	assertTrue (Poco::File(preferencesFile()).exists());
}


void SampleProgramsTest::testBundleServerConfigFileTakesPrecedence()
{
	const std::vector<std::string> args{CONFIG_FILE_OPTION + writeConfiguration()};
	Poco::Pipe outPipe;
	Poco::ProcessHandle server = Poco::Process::launch(OSP_SAMPLES_BUNDLE_SERVER, args, nullptr, &outPipe, &outPipe);
	Poco::PipeInputStream output(outPipe);
	bool started = false;
	Poco::Event startedOrEnded;
	// Reads the output to its end, so that the server never waits for a full pipe.
	std::thread reader([&output, &started, &startedOrEnded]()
		{
			std::string line;
			while (std::getline(output, line))
			{
				if (line.find("Startup complete."s) != std::string::npos)
				{
					started = true;
					startedOrEnded.set();
				}
			}
			startedOrEnded.set();
		});
	startedOrEnded.tryWait(static_cast<long>(STARTUP_TIMEOUT.count()));
	// The bundles have created the code cache and the preferences directory.
	const bool codeCacheCreated = Poco::File(Poco::Path(Poco::Path::forDirectory(_pDirectory->path()), "codeCache"s)).exists();
	const bool preferencesCreated = Poco::File(Poco::Path(preferencesFile()).parent()).exists();
	// A termination request that arrives before the server waits for one
	// would end it with SIGINT; the test needs no orderly shutdown.
#if defined(POCO_OS_FAMILY_WINDOWS)
	// On Windows, kill() closes the handle, so that it cannot be waited for, and
	// fails for a server that has ended; the reader ends when the server has ended.
	try
	{
		Poco::Process::kill(server);
	}
	catch (Poco::SystemException&)
	{
	}
#else
	Poco::Process::kill(server);
	server.wait();
#endif
	reader.join();

	assertTrue (started);
	assertTrue (codeCacheCreated);
	assertTrue (preferencesCreated);
}


std::string SampleProgramsTest::writeConfiguration()
{
	const Poco::Path directory(Poco::Path::forDirectory(_pDirectory->path()));
	const Poco::Path repository(directory, "bundles"s);
	TestApplication::copyBundles({PREFERENCES_SAMPLE}, repository.toString());

	const Poco::Path configuration(directory, "test.properties"s);
	// Saved by the configuration, which escapes the backslashes of Windows paths.
	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfiguration = new Poco::Util::PropertyFileConfiguration;
	pConfiguration->setString("osp.codeCache"s, Poco::Path(directory, "codeCache"s).toString());
	pConfiguration->setString("osp.data"s, Poco::Path(directory, "data"s).toString());
	pConfiguration->setString("osp.bundleRepository"s, repository.toString());
	pConfiguration->setString("logging.loggers.root.level"s, "information"s);
	pConfiguration->save(configuration.toString());
	return configuration.toString();
}


std::string SampleProgramsTest::preferencesFile() const
{
	Poco::Path path(Poco::Path::forDirectory(_pDirectory->path()));
	path.pushDirectory("data"s);
	path.pushDirectory(Poco::OSP::PreferencesService::SERVICE_NAME);
	path.setFileName(PREFERENCES_SAMPLE + ".properties"s);
	return path.toString();
}


CppUnit::Test* SampleProgramsTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SampleProgramsTest");

	CppUnit_addTest(pSuite, SampleProgramsTest, testBundleContainerConfigFileTakesPrecedence);
	CppUnit_addTest(pSuite, SampleProgramsTest, testBundleServerConfigFileTakesPrecedence);

	return pSuite;
}
