//
// LogTestResult.cpp
//
// Library: OSP/Unit
// Package: Unit
// Module:  LogTestResult
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Unit/LogTestResult.h"
#include "CppUnit/TextTestResult.h"


using CppUnit::TextTestResult;


namespace Poco::OSP::Unit {


LogTestResult::LogTestResult(std::ostream& ostr): 
	TextTestResult(ostr),
	_ostr(ostr)
{
}


LogTestResult::~LogTestResult() = default;


void LogTestResult::startTest(CppUnit::Test* test)
{
	TextTestResult::startTest(test);
	_ostr << std::endl;
}


void LogTestResult::print(std::ostream& stream)
{
	TextTestResult::print(stream);
	stream << std::endl;
}


void LogTestResult::printErrors(std::ostream& stream)
{
	TextTestResult::printErrors(stream);
	stream << std::endl;
}


void LogTestResult::printFailures(std::ostream& stream)
{
	TextTestResult::printFailures(stream);
	stream << std::endl;
}


void LogTestResult::printHeader(std::ostream& stream)
{
	TextTestResult::printHeader(stream);
	stream << std::endl;
}


} // namespace Poco::OSP::Unit
