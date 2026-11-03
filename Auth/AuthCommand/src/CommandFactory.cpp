//
// CommandFactory.cpp
//
// Library: OSP/Auth
// Package: Shell
// Module:  CommandFactory
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthCommand.h"
#include "PasswdCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/ClassLibrary.h"


class AuthCommandFactory: public Poco::OSP::Shell::CommandFactory
	/// Creates the auth command.
{
public:
	AuthCommandFactory() = default;
	~AuthCommandFactory() override = default;

	[[nodiscard]] Poco::OSP::Shell::Command* createCommand() override;
		/// Returns a new AuthCommand.
};


class PasswdCommandFactory: public Poco::OSP::Shell::CommandFactory
	/// Creates the passwd command.
{
public:
	PasswdCommandFactory() = default;
	~PasswdCommandFactory() override = default;

	[[nodiscard]] Poco::OSP::Shell::Command* createCommand() override;
		/// Returns a new PasswdCommand.
};


Poco::OSP::Shell::Command* AuthCommandFactory::createCommand()
{
	return new AuthCommand(context());
}


Poco::OSP::Shell::Command* PasswdCommandFactory::createCommand()
{
	return new PasswdCommand(context());
}


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(AuthCommandFactory)
	POCO_EXPORT_CLASS(PasswdCommandFactory)
POCO_END_MANIFEST
