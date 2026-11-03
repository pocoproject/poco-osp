//
// CommandFactory.cpp
//
// Copyright (c) 2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Shell/CommandFactory.h"
#include "Poco/ClassLibrary.h"
#include "BundlesCommand.h"
#include "ServicesCommand.h"
#include "StartCommand.h"
#include "StopCommand.h"
#include "ResolveCommand.h"
#include "UninstallCommand.h"
#include "InfoCommand.h"
#include "InstallCommand.h"
#include "UpgradeCommand.h"


namespace Poco::OSP::Shell {


template <class Cmd>
class CommandFactoryImpl: public CommandFactory
{
public:
	[[nodiscard]] Command* createCommand() override
	{
		return new Cmd(context());
	}
};


using BundlesCommandFactory = CommandFactoryImpl<BundlesCommand>;
using ServicesCommandFactory = CommandFactoryImpl<ServicesCommand>;
using StartCommandFactory = CommandFactoryImpl<StartCommand>;
using StopCommandFactory = CommandFactoryImpl<StopCommand>;
using ResolveCommandFactory = CommandFactoryImpl<ResolveCommand>;
using UninstallCommandFactory = CommandFactoryImpl<UninstallCommand>;
using InfoCommandFactory = CommandFactoryImpl<InfoCommand>;
using InstallCommandFactory = CommandFactoryImpl<InstallCommand>;
using UpgradeCommandFactory = CommandFactoryImpl<UpgradeCommand>;


} // namespace Poco::OSP::Shell


POCO_BEGIN_NAMED_MANIFEST(Shell, Poco::OSP::Shell::CommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::BundlesCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::ServicesCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::StartCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::StopCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::ResolveCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::UninstallCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::InfoCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::InstallCommandFactory)
	POCO_EXPORT_CLASS(Poco::OSP::Shell::UpgradeCommandFactory)
POCO_END_MANIFEST
