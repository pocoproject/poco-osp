//
// QLExpr.cpp
//
// Library: OSP
// Package: Util
// Module:  QLExpr
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/QLExpr.h"
#include "Poco/OSP/Properties.h"
#include "Poco/NumberParser.h"
#include "Poco/String.h"
#include "Poco/Glob.h"
#include "Poco/Exception.h"
#include <algorithm>
#include <typeinfo>


using Poco::NumberParser;
using Poco::AnyCast;
using namespace std::string_literals;


namespace Poco {
namespace OSP {


//
// QLExpr
//


QLExpr::QLExpr()
{
}


QLExpr::~QLExpr()
{
}


//
// QLAndExpr
//


QLAndExpr::QLAndExpr(QLExpr::Ptr pLeft, QLExpr::Ptr pRight):
	_pLeft(pLeft),
	_pRight(pRight)
{
}


QLAndExpr::~QLAndExpr()
{
}


bool QLAndExpr::evaluate(const Properties& props) const
{
	return _pLeft->evaluate(props) && _pRight->evaluate(props);
}


//
// QLOrExpr
//


QLOrExpr::QLOrExpr(QLExpr::Ptr pLeft, QLExpr::Ptr pRight):
	_pLeft(pLeft),
	_pRight(pRight)
{
}


QLOrExpr::~QLOrExpr()
{
}


bool QLOrExpr::evaluate(const Properties& props) const
{
	return _pLeft->evaluate(props) || _pRight->evaluate(props);
}


//
// QLNotExpr
//


QLNotExpr::QLNotExpr(QLExpr::Ptr pExpr):
	_pExpr(pExpr)
{
}


QLNotExpr::~QLNotExpr()
{
}


bool QLNotExpr::evaluate(const Properties& props) const
{
	return !_pExpr->evaluate(props);
}


//
// QLExistsExpr
//


QLExistsExpr::QLExistsExpr(const std::string& prop):
	_prop(prop)
{
}


QLExistsExpr::~QLExistsExpr()
{
}


bool QLExistsExpr::evaluate(const Properties& props) const
{
	return props.has(_prop);
}


//
// QLRelExpr
//


bool QLRelExpr::evaluate(const Properties& props) const
{
	try
	{
		return evaluateImpl(props);
	}
	catch (Poco::Exception&)
	{
		return false;
	}
}


//
// QLEqExpr
//


QLEqExpr::QLEqExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLEqExpr::~QLEqExpr()
{
}


bool QLEqExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) == AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) == AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) == AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) == AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLNeExpr
//


QLNeExpr::QLNeExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLNeExpr::~QLNeExpr()
{
}


bool QLNeExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) != AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) != AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) != AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) != AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLLtExpr
//


QLLtExpr::QLLtExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLLtExpr::~QLLtExpr()
{
}


bool QLLtExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) < AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) < AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) < AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) < AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLLeExpr
//


QLLeExpr::QLLeExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLLeExpr::~QLLeExpr()
{
}


bool QLLeExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) <= AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) <= AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) <= AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) <= AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLGtExpr
//


QLGtExpr::QLGtExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLGtExpr::~QLGtExpr()
{
}


bool QLGtExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) > AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) > AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) > AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) > AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLGeExpr
//


QLGeExpr::QLGeExpr(const std::string& prop, const Poco::Any& value):
	_prop(prop),
	_value(value)
{
}


QLGeExpr::~QLGeExpr()
{
}


bool QLGeExpr::evaluateImpl(const Properties& props) const
{
	if (_value.type() == typeid(int))
		return props.getInt(_prop, 0) >= AnyCast<int>(_value);
	else if (_value.type() == typeid(double))
		return props.getFloat(_prop, 0) >= AnyCast<double>(_value);
	else if (_value.type() == typeid(bool))
		return props.getBool(_prop, false) >= AnyCast<bool>(_value);
	else if (_value.type() == typeid(std::string))
		return props.get(_prop, ""s) >= AnyCast<std::string>(_value);
	else
		return false;
}


//
// QLMatchExpr
//


QLMatchExpr::QLMatchExpr(const std::string& prop, const std::string& expr):
	_prop(prop),
	_glob(expr)
{
	// Queries also come from users (the shell's services command).
	if (std::count(expr.begin(), expr.end(), '*') > MAX_WILDCARDS)
		throw Poco::SyntaxException("Too many wildcards in pattern", expr);
}


QLMatchExpr::~QLMatchExpr()
{
}


bool QLMatchExpr::evaluate(const Properties& props) const
{
	std::string prop(props.get(_prop, ""s));
	return _glob.match(prop);
}


//
// QLMatchRegExpr
//


QLMatchRegExpr::QLMatchRegExpr(const std::string& prop, const std::string& expr):
	_prop(prop),
	_expr(expr)
{
}


QLMatchRegExpr::~QLMatchRegExpr()
{
}


bool QLMatchRegExpr::evaluate(const Properties& props) const
{
	std::string prop(props.get(_prop, ""s));
	return _expr.match(prop);
}


//
// QLTrueExpr
//


QLTrueExpr::QLTrueExpr()
{
}


bool QLTrueExpr::evaluate(const Properties& props) const
{
	return true;
}


} } // namespace Poco::OSP
