//
// StatementPlaceholderProvider.h
//
// Library: OSP/Auth/Data
// Package: Auth
// Module:  StatementPlaceholderProvider
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Data_StatementPlaceholderProvider_INCLUDED
#define OSP_Auth_Data_StatementPlaceholderProvider_INCLUDED


#include "Poco/OSP/Auth/Data/Data.h"
#include <string>
#include <memory>


namespace Poco::OSP::Auth::Data {


class OSPAuthData_API StatementPlaceholderProvider
{
public:
	using Ptr = std::unique_ptr<StatementPlaceholderProvider>;

	virtual void reset() = 0;
	virtual std::string next() = 0;
	virtual char quote() = 0;

	virtual ~StatementPlaceholderProvider();
};


class OSPAuthData_API DefaultStatementPlaceholderProvider: public StatementPlaceholderProvider
{
public:
	void reset() override;
	std::string next() override;
	char quote() override;
};


class OSPAuthData_API PostgresStatementPlaceholderProvider: public StatementPlaceholderProvider
{
public:
	void reset() override;
	std::string next() override;
	char quote() override;

private:
	int _n = 1;
};


class OSPAuthData_API MySQLStatementPlaceholderProvider: public DefaultStatementPlaceholderProvider
{
public:
	char quote() override;
};


} // namespace Poco::OSP::Auth::Data


#endif // OSP_Auth_Data_StatementPlaceholderProvider_INCLUDED
