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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/QuitCommandFactory.h"
#include "Poco/OSP/Shell/QuitCommand.h"


namespace Poco {
namespace OSP {
namespace Shell {


QuitCommandFactory::QuitCommandFactory()
{
}


QuitCommandFactory::~QuitCommandFactory()
{
}


Command* QuitCommandFactory::createCommand()
{
	return new QuitCommand(context());
}


} } } // namespace Poco::OSP::Shell
