//
// ResponseStreamTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ResponseStreamTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/ResponseStream.h"
#include "Poco/OSP/Shell/ShellConnection.h"
#include <ostream>
#include <sstream>
#include <string>


using Poco::OSP::Shell::ResponseStream;
using Poco::OSP::Shell::ShellConnection;
using namespace std::string_literals;


ResponseStreamTest::ResponseStreamTest(const std::string& name): CppUnit::TestCase(name)
{
}


ResponseStreamTest::~ResponseStreamTest() = default;


void ResponseStreamTest::setUp()
{
}


void ResponseStreamTest::tearDown()
{
}


void ResponseStreamTest::testSingleLineBlock()
{
	std::ostringstream ostr;
	ResponseStream response(ostr, ShellConnection::STATUS_OK);
	response << "OK" << std::flush;
	assertEqual ("200 OK\r\n"s, ostr.str());
}


void ResponseStreamTest::testMultiLineBlock()
{
	std::ostringstream ostr;
	ResponseStream response(ostr, ShellConnection::STATUS_INTERMEDIATE);
	response << "first\nsecond\nthird\n" << std::flush;
	assertEqual ("100-first\r\n100-second\r\n100 third\r\n"s, ostr.str());
}


void ResponseStreamTest::testStatusChangeEndsBlock()
{
	std::ostringstream ostr;
	ResponseStream response(ostr, ShellConnection::STATUS_INTERMEDIATE);
	response << "one\ntwo\n";
	response.setStatus(ShellConnection::STATUS_CLIENT_DENIED);
	response << "DENIED\n" << std::flush;
	assertEqual ("100-one\r\n100 two\r\n403 DENIED\r\n"s, ostr.str());
}


void ResponseStreamTest::testFlushEndsBlock()
{
	std::ostringstream ostr;
	ResponseStream response(ostr, ShellConnection::STATUS_INTERMEDIATE);
	response << "one" << std::flush << "two\n" << std::flush;
	assertEqual ("100 one\r\n100 two\r\n"s, ostr.str());
}


void ResponseStreamTest::testGetStatus()
{
	std::ostringstream ostr;
	ResponseStream response(ostr, ShellConnection::STATUS_INTERMEDIATE);
	assertEqual (static_cast<int>(ShellConnection::STATUS_INTERMEDIATE), response.getStatus());
	response.setStatus(ShellConnection::STATUS_OK);
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), response.getStatus());
	assertEqual (static_cast<int>(ShellConnection::STATUS_OK), response.rdbuf()->getStatus());
}


CppUnit::Test* ResponseStreamTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ResponseStreamTest");

	CppUnit_addTest(pSuite, ResponseStreamTest, testSingleLineBlock);
	CppUnit_addTest(pSuite, ResponseStreamTest, testMultiLineBlock);
	CppUnit_addTest(pSuite, ResponseStreamTest, testStatusChangeEndsBlock);
	CppUnit_addTest(pSuite, ResponseStreamTest, testFlushEndsBlock);
	CppUnit_addTest(pSuite, ResponseStreamTest, testGetStatus);

	return pSuite;
}
