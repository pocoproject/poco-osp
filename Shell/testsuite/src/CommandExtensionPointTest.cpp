//
// CommandExtensionPointTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "CommandExtensionPointTest.h"
#include "ShellFixture.h"
#include "LogCapture.h"
#include "CommandExtensionPoint.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/Element.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include "Poco/Message.h"
#include "Poco/Platform.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Thread.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#if defined(POCO_OS_FAMILY_UNIX)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::CommandExtensionPoint;
using Poco::OSP::Shell::Session;
using namespace std::string_literals;


CommandExtensionPointTest::CommandExtensionPointTest(const std::string& name): CppUnit::TestCase(name)
{
}


CommandExtensionPointTest::~CommandExtensionPointTest() = default;


void CommandExtensionPointTest::setUp()
{
	_pFixture = std::make_unique<ShellFixture>(""s);
}


void CommandExtensionPointTest::tearDown()
{
	_pFixture.reset();
}


void CommandExtensionPointTest::testDuplicateCommandIsLoggedAndNotAdded()
{
	LogCapture capture(_pFixture->shellContext()->logger().name());
	Poco::AutoPtr<Poco::XML::Document> pDocument(new Poco::XML::Document);
	Poco::AutoPtr<Poco::XML::Element> pElement(pDocument->createElement("extension"s));
	pElement->setAttribute(Poco::OSP::ExtensionPointService::POINT_ATTR, CommandExtensionPoint::EXTPOINT);
	pElement->setAttribute("command"s, "loglevel"s);
	pElement->setAttribute("class"s, "LogLevelCommandFactory"s);
	pElement->setAttribute("library"s, ShellFixture::LOGLEVEL_BUNDLE);
	_pFixture->extensionPoint().handleExtension(_pFixture->bundle(ShellFixture::OTHER_BUNDLE), pElement);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (static_cast<std::size_t>(1), messages.size());
	assertEqual (static_cast<int>(Poco::Message::PRIO_ERROR), static_cast<int>(messages[0].getPriority()));
	assertEqual ("The shell command loglevel of bundle osp.shell.test.other is not added: "
		"a command with this name exists already."s, messages[0].getText());

	_pFixture->bundle(ShellFixture::OTHER_BUNDLE)->stop();
	Session session(_pFixture->shellContext(), ""s);
	assertTrue (session.login("user"s, ""s));
	std::ostringstream ostr;
	assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, "loglevel"s, ostr));
}


void CommandExtensionPointTest::testCommandsOfStoppedBundleAreRemoved()
{
	_pFixture->bundle(ShellFixture::LOGLEVEL_BUNDLE)->stop();
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertTrue (std::find(commands.begin(), commands.end(), "loglevel"s) == commands.end());
	assertTrue (std::find(commands.begin(), commands.end(), "bundles"s) != commands.end());
}


void CommandExtensionPointTest::testCommandsOfStoppedBundleAreNotAdded()
{
	_pFixture->bundle(ShellFixture::LOGLEVEL_BUNDLE)->stop();
	_pFixture->addExtensions(ShellFixture::LOGLEVEL_BUNDLE);
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertTrue (std::find(commands.begin(), commands.end(), "loglevel"s) == commands.end());
}


void CommandExtensionPointTest::testCommandRunsToEndWhenItsBundleStops()
{
#if defined(POCO_OS_FAMILY_UNIX)
	// The install command of the commands bundle reads the bundle from a FIFO,
	// so it waits in the code of the commands library until the test writes.
	Poco::TemporaryFile fifo;
	assertEqual (0, ::mkfifo(fifo.path().c_str(), S_IRUSR | S_IWUSR));
	Session session(_pFixture->shellContext(), ""s);
	assertTrue (session.login("user"s, ""s));
	std::ostringstream ostr;
	std::atomic<int> status{-1};
	Poco::Thread runner;
	runner.startFunc([this, &session, &fifo, &ostr, &status]()
	{
		status = _pFixture->processor().execute(session, "install file://"s + fifo.path(), ostr);
	});

	// Opening the FIFO for writing succeeds once the command has opened it for reading.
	constexpr long RETRY_INTERVAL_MS = 10;
	const auto openForWriting = [&fifo]()
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX open()
		return ::open(fifo.path().c_str(), O_WRONLY | O_NONBLOCK);
	};
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	int fd = openForWriting();
	while (fd < 0 && errno == ENXIO && std::chrono::steady_clock::now() < deadline)
	{
		Poco::Thread::sleep(RETRY_INTERVAL_MS);
		fd = openForWriting();
	}
	assertTrue (fd >= 0);
	_pFixture->bundle(ShellFixture::COMMANDS_BUNDLE)->stop();
	const std::string data("not a bundle"s);
	assertEqual (static_cast<ssize_t>(data.size()), ::write(fd, data.data(), data.size()));
	::close(fd);
	runner.join();

	assertEqual (static_cast<int>(Command::STATUS_ERROR), status.load());
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertTrue (std::find(commands.begin(), commands.end(), "install"s) == commands.end());
#endif
}


void CommandExtensionPointTest::testFactoryClassComesFromBundleLibrary()
{
	// Both libraries export LibraryNameCommandFactory; each command writes the name of its library.
	_pFixture->addCommandBundle("osp.shell.test.first"s, "testsuite/bundles/first"s, OSPSHELL_TEST_FIRST_LIBRARY);
	_pFixture->addCommandBundle("osp.shell.test.second"s, "testsuite/bundles/second"s, OSPSHELL_TEST_SECOND_LIBRARY);

	Session session(_pFixture->shellContext(), ""s);
	assertTrue (session.login("user"s, ""s));
	for (const auto& name: {"first"s, "second"s})
	{
		std::ostringstream ostr;
		assertEqual (static_cast<int>(Command::STATUS_OK), _pFixture->processor().execute(session, name, ostr));
		assertEqual (name, ostr.str());
	}
}


void CommandExtensionPointTest::testFactoryClassNotInBundleLibraryIsNotFound()
{
	// LogLevelCommandFactory is exported by the library of the loglevel bundle only.
	Poco::AutoPtr<Poco::XML::Document> pDocument(new Poco::XML::Document);
	Poco::AutoPtr<Poco::XML::Element> pElement(pDocument->createElement("extension"s));
	pElement->setAttribute(Poco::OSP::ExtensionPointService::POINT_ATTR, CommandExtensionPoint::EXTPOINT);
	pElement->setAttribute("command"s, "otherloglevel"s);
	pElement->setAttribute("class"s, "LogLevelCommandFactory"s);
	pElement->setAttribute("library"s, ShellFixture::COMMANDS_BUNDLE);
	try
	{
		_pFixture->extensionPoint().handleExtension(_pFixture->bundle(ShellFixture::COMMANDS_BUNDLE), pElement);
		fail("a class that the library does not export must not be used");
	}
	catch (Poco::NotFoundException&)
	{
	}
	std::vector<std::string> commands;
	_pFixture->processor().listCommands(commands);
	assertTrue (std::find(commands.begin(), commands.end(), "otherloglevel"s) == commands.end());
}


CppUnit::Test* CommandExtensionPointTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("CommandExtensionPointTest");

	CppUnit_addTest(pSuite, CommandExtensionPointTest, testDuplicateCommandIsLoggedAndNotAdded);
	CppUnit_addTest(pSuite, CommandExtensionPointTest, testCommandsOfStoppedBundleAreRemoved);
	CppUnit_addTest(pSuite, CommandExtensionPointTest, testCommandsOfStoppedBundleAreNotAdded);
	CppUnit_addTest(pSuite, CommandExtensionPointTest, testFactoryClassComesFromBundleLibrary);
	CppUnit_addTest(pSuite, CommandExtensionPointTest, testFactoryClassNotInBundleLibraryIsNotFound);
#if defined(POCO_OS_FAMILY_UNIX)
	CppUnit_addTest(pSuite, CommandExtensionPointTest, testCommandRunsToEndWhenItsBundleStops);
#endif

	return pSuite;
}
