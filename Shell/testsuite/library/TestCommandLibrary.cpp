//
// TestCommandLibrary.cpp
//
// A command library for the tests of the command extension point, built twice
// with different values of OSPSHELL_TEST_LIBRARY_NAME.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/ClassLibrary.h"
#include <ostream>
#include <string>
#include <vector>


class LibraryNameCommand: public Poco::OSP::Shell::Command
	/// Writes the name of the library that it comes from.
{
public:
	LibraryNameCommand() = default;

	~LibraryNameCommand() override = default;

	LibraryNameCommand(const LibraryNameCommand&) = delete;
	LibraryNameCommand(LibraryNameCommand&&) = delete;
	LibraryNameCommand& operator = (const LibraryNameCommand&) = delete;
	LibraryNameCommand& operator = (LibraryNameCommand&&) = delete;

	[[nodiscard]] int execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args,
		std::ostream& ostr) override;
};


int LibraryNameCommand::execute([[maybe_unused]] Poco::OSP::Shell::Session& session,
	[[maybe_unused]] const std::vector<std::string>& args, std::ostream& ostr)
{
	ostr << OSPSHELL_TEST_LIBRARY_NAME;
	return STATUS_OK;
}


class LibraryNameCommandFactory: public Poco::OSP::Shell::CommandFactory
	/// Creates LibraryNameCommand objects.
{
public:
	LibraryNameCommandFactory() = default;

	~LibraryNameCommandFactory() override = default;

	LibraryNameCommandFactory(const LibraryNameCommandFactory&) = delete;
	LibraryNameCommandFactory(LibraryNameCommandFactory&&) = delete;
	LibraryNameCommandFactory& operator = (const LibraryNameCommandFactory&) = delete;
	LibraryNameCommandFactory& operator = (LibraryNameCommandFactory&&) = delete;

	[[nodiscard]] Poco::OSP::Shell::Command* createCommand() override;
};


Poco::OSP::Shell::Command* LibraryNameCommandFactory::createCommand()
{
	return new LibraryNameCommand;
}


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(LibraryNameCommandFactory)
POCO_END_MANIFEST
