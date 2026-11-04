//
// LogTestResult.h
//
// Library: OSP/Unit
// Package: Unit
// Module:  LogTestResult
//
// Definition of the LogTestResult class.
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Unit_LogTestResult_INCLUDED
#define OSP_Unit_LogTestResult_INCLUDED


#include "CppUnit/TextTestResult.h"
#include "Poco/OSP/Unit/Unit.h"
#include <ostream>


namespace Poco::OSP::Unit {


class OSPUnit_API LogTestResult: public CppUnit::TextTestResult
	/// The LogTestResult class. The purpose of this class is to extend the
	/// CppUnit TextTestResult and bypass default output to std::cout.
{
public:	
	LogTestResult() = delete;

	LogTestResult(std::ostream& ostr);
		/// Creates a LogTestResult with supplied ostream as destination.
		/// Typically, ostr will be a Poco::LogStream.

	~LogTestResult() override;
		/// Destroys the LogTestResult.
	
	void startTest(CppUnit::Test* test) override;
		/// Calls TextTestResult::startTest() and appends std::endl to the stream.
	
	void print(std::ostream& stream) override;
		/// Calls TextTestResult::print() and appends std::endl to the stream.

	void printErrors(std::ostream& stream) override;
		/// Calls TextTestResult::printErrors() and appends std::endl to the stream.

	void printFailures(std::ostream& stream) override;
		/// Calls TextTestResult::printFailures() and appends std::endl to the stream.

	void printHeader(std::ostream& stream) override;
		/// Calls TextTestResult::printHeader() and appends std::endl to the stream.

private:
	std::ostream& _ostr;
};


} // namespace Poco::OSP::Unit


#endif // OSP_Unit_LogTestResult_INCLUDED
