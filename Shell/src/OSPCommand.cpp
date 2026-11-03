//
// OSPCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  OSPCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/NumberParser.h"
#include "Poco/Glob.h"
#include <set>
#include <vector>


using Poco::NumberParser;


namespace Poco::OSP::Shell {


OSPCommand::OSPCommand(Poco::OSP::BundleContext::Ptr pContext, const std::string& name):
	AbstractCommand(
		pContext->thisBundle()->properties().getString(name + ".description"),
		pContext->thisBundle()->properties().getString(name + ".usage")),
	_pContext(pContext)
{
}


OSPCommand::~OSPCommand() = default;


Poco::OSP::Bundle::Ptr OSPCommand::findBundle(const std::string& symbolicNameOrId) const
{
	int id;
	if (NumberParser::tryParse(symbolicNameOrId, id))
	{
		return _pContext->findBundle(id);
	}
	else
	{
		return _pContext->findBundle(symbolicNameOrId);
	}
}


bool OSPCommand::isRequiredByShell(const Poco::OSP::Bundle::Ptr& pBundle) const
{
	std::vector<Poco::OSP::Bundle::Ptr> pending{_pContext->thisBundle()};
	std::set<int> visited;
	while (!pending.empty())
	{
		const Poco::OSP::Bundle::Ptr pRequired = pending.back();
		pending.pop_back();
		if (pRequired->id() == pBundle->id()) return true;
		if (!visited.insert(pRequired->id()).second) continue;

		for (const auto& dependency: pRequired->resolvedDependencies())
		{
			Poco::OSP::Bundle::Ptr pDependency = _pContext->findBundle(dependency.id);
			if (pDependency != nullptr) pending.push_back(pDependency);
		}
	}
	return false;
}


bool OSPCommand::match(const std::string& pattern, const std::string& subject)
{
	if (pattern.empty())
	{
		return true;
	}
	else
	{
		Poco::Glob glob(pattern);
		return glob.match(subject);
	}
}


bool OSPCommand::match(const std::vector<std::string>& patterns, const std::string& subject)
{
	if (patterns.empty())
	{
		return true;
	}
	else
	{
		for (std::vector<std::string>::const_iterator it = patterns.begin(); it != patterns.end(); ++it)
		{
			Poco::Glob glob(*it);
			if (glob.match(subject))
				return true;
		}
		return false;
	}
}


} // namespace Poco::OSP::Shell
