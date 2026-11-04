//
// TestCommandFactory.h
//
// Definition of the TestCommandFactory class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestCommandFactory_INCLUDED
#define TestCommandFactory_INCLUDED


#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include <functional>
#include <ostream>
#include <string>
#include <vector>


class TestCommandFactory: public Poco::OSP::Shell::CommandFactory
	/// Creates commands that call a given function.
{
public:
	using Function = std::function<int(Poco::OSP::Shell::Session&, const std::vector<std::string>&, std::ostream&)>;

	TestCommandFactory() = delete;

	explicit TestCommandFactory(Function function);
		/// Creates the TestCommandFactory. The commands it creates call the
		/// given function with the arguments of Command::execute().

	~TestCommandFactory() override;
		/// Destroys the TestCommandFactory.

	TestCommandFactory(const TestCommandFactory&) = delete;
	TestCommandFactory(TestCommandFactory&&) = delete;
	TestCommandFactory& operator = (const TestCommandFactory&) = delete;
	TestCommandFactory& operator = (TestCommandFactory&&) = delete;

	// CommandFactory
	[[nodiscard]] Poco::OSP::Shell::Command* createCommand() override;

private:
	Function _function;
};


#endif // TestCommandFactory_INCLUDED
