//
// SampleProgramsTest.h
//
// Definition of the SampleProgramsTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SampleProgramsTest_INCLUDED
#define SampleProgramsTest_INCLUDED


#include "CppUnit/TestCase.h"
#include "Poco/TemporaryFile.h"
#include <memory>
#include <string>


class SampleProgramsTest: public CppUnit::TestCase
	/// Runs the BundleServer and BundleContainer programs.
{
public:
	SampleProgramsTest() = delete;
	explicit SampleProgramsTest(const std::string& name);
	~SampleProgramsTest() override;

	void setUp() override;
	void tearDown() override;

	void testBundleContainerConfigFileTakesPrecedence();
	void testBundleServerConfigFileTakesPrecedence();

	static CppUnit::Test* suite();

private:
	[[nodiscard]] std::string writeConfiguration();
		/// Writes a configuration file for the programs that runs the
		/// Preferences sample with the code cache and the data in the
		/// temporary directory, and returns its path.

	[[nodiscard]] std::string preferencesFile() const;
		/// Returns the path of the file in which the Preferences sample
		/// stores its preferences, in the data directory of the
		/// configuration.

	std::unique_ptr<Poco::TemporaryFile> _pDirectory;
};


#endif // SampleProgramsTest_INCLUDED
