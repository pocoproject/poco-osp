//
// StatementPlaceholderProvider.cpp
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  StatementPlaceholderProvider
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/Data/StatementPlaceholderProvider.h"
#include "Poco/Format.h"


using namespace std::string_literals;


namespace Poco::OSP::Auth::Data {


StatementPlaceholderProvider::~StatementPlaceholderProvider() = default;


void DefaultStatementPlaceholderProvider::reset()
{
}


std::string DefaultStatementPlaceholderProvider::next()
{
	return "?"s;
}


char DefaultStatementPlaceholderProvider::quote()
{
	return '"';
}


void PostgresStatementPlaceholderProvider::reset()
{
	_n = 1;
}


std::string PostgresStatementPlaceholderProvider::next()
{
	return Poco::format("$%d"s, _n++);
}


char PostgresStatementPlaceholderProvider::quote()
{
	return '"';
}


char MySQLStatementPlaceholderProvider::quote()
{
	return '`';
}


} // namespace Poco::OSP::Auth::Data
