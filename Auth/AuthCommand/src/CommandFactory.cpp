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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "AuthCommand.h"
#include "PasswdCommand.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/ClassLibrary.h"


class AuthCommandFactory: public Poco::OSP::Shell::CommandFactory
{
public:
	Poco::OSP::Shell::Command* createCommand()
	{
		return new AuthCommand(context());
	}
};


class PasswdCommandFactory: public Poco::OSP::Shell::CommandFactory
{
public:
	Poco::OSP::Shell::Command* createCommand()
	{
		return new PasswdCommand(context());
	}
};


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(AuthCommandFactory)
	POCO_EXPORT_CLASS(PasswdCommandFactory)
POCO_END_MANIFEST
