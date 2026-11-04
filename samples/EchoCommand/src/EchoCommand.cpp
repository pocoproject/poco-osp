//
// EchoCommand.cpp
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/ClassLibrary.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/OSP/Shell/Session.h"
#include <cstddef>


namespace
{
	class EchoCommand: public Poco::OSP::Shell::Command
		/// Outputs the arguments, separated by spaces.
		///
		/// Registered without a permission, so it checks itself that the user has logged in.
	{
	public:
		[[nodiscard]] int execute(Poco::OSP::Shell::Session& session, const std::vector<std::string>& args,
			std::ostream& ostr) override
		{
			if (!session.authenticated()) return Command::STATUS_DENIED;

			if (args.size() == 2 && args[1] == "--help")
			{
				ostr << "usage: " << args[0] << " [<arg>...]\n"
				     << "Echo arguments to output.\n";
			}
			else
			{
				for (std::size_t i = 1; i < args.size(); ++i)
				{
					if (i > 1) ostr << ' ';
					ostr << args[i];
				}
				ostr << '\n';
			}
			return Command::STATUS_OK;
		}
	};


	class EchoCommandFactory: public Poco::OSP::Shell::CommandFactory
	{
	public:
		[[nodiscard]] Poco::OSP::Shell::Command* createCommand() override
		{
			return new EchoCommand;
		}
	};
}


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(EchoCommandFactory)
POCO_END_MANIFEST
