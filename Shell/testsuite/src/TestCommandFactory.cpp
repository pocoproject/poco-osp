//
// TestCommandFactory.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestCommandFactory.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include <ostream>
#include <string>
#include <utility>
#include <vector>


using Poco::OSP::Shell::Command;
using Poco::OSP::Shell::Session;


namespace
{
	class FunctionCommand: public Command
		/// A command that calls a function.
	{
	public:
		FunctionCommand() = delete;

		explicit FunctionCommand(TestCommandFactory::Function function):
			_function(std::move(function))
		{
		}

		~FunctionCommand() override = default;

		FunctionCommand(const FunctionCommand&) = delete;
		FunctionCommand(FunctionCommand&&) = delete;
		FunctionCommand& operator = (const FunctionCommand&) = delete;
		FunctionCommand& operator = (FunctionCommand&&) = delete;

		[[nodiscard]] int execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr) override
		{
			return _function(session, args, ostr);
		}

	private:
		TestCommandFactory::Function _function;
	};
}


TestCommandFactory::TestCommandFactory(Function function):
	_function(std::move(function))
{
}


TestCommandFactory::~TestCommandFactory() = default;


Command* TestCommandFactory::createCommand()
{
	return new FunctionCommand(_function);
}
