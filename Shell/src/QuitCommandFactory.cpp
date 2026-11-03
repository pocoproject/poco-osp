//
// QuitCommandFactory.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  QuitCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/QuitCommandFactory.h"
#include "Poco/OSP/Shell/QuitCommand.h"


namespace Poco::OSP::Shell {


QuitCommandFactory::QuitCommandFactory() = default;


QuitCommandFactory::~QuitCommandFactory() = default;


Command* QuitCommandFactory::createCommand()
{
	return new QuitCommand(context());
}


} // namespace Poco::OSP::Shell
