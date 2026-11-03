//
// QLExpr.h
//
// Library: OSP
// Package: Util
// Module:  QLExpr
//
// Definition of the QLExpr class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_QLExpr_INCLUDED
#define OSP_QLExpr_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/RefCountedObject.h"
#include "Poco/AutoPtr.h"
#include "Poco/Any.h"
#include "Poco/RegularExpression.h"
#include "Poco/Glob.h"


namespace Poco::OSP {


class Properties;


class OSP_API QLExpr: public Poco::RefCountedObject
	/// The base class for all Query Language expressions.
{
public:
	using Ptr = Poco::AutoPtr<QLExpr>;
	using ConstPtr = const Ptr;

	[[nodiscard]] virtual bool evaluate(const Properties& props) const = 0;
		/// Evaluates the expression on the given properties.

protected:
	QLExpr();
	~QLExpr() override;
};


class OSP_API QLAndExpr: public QLExpr
{
public:
	QLAndExpr(QLExpr::Ptr pLeft, QLExpr::Ptr pRight);
	~QLAndExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

private:
	QLExpr::Ptr _pLeft;
	QLExpr::Ptr _pRight;
};


class OSP_API QLOrExpr: public QLExpr
{
public:
	QLOrExpr(QLExpr::Ptr pLeft, QLExpr::Ptr pRight);
	~QLOrExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

private:
	QLExpr::Ptr _pLeft;
	QLExpr::Ptr _pRight;
};


class OSP_API QLNotExpr: public QLExpr
{
public:
	QLNotExpr(QLExpr::Ptr pExpr);
	~QLNotExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

private:
	QLExpr::Ptr _pExpr;
};


class OSP_API QLExistsExpr: public QLExpr
{
public:
	QLExistsExpr(const std::string& prop);
	~QLExistsExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

private:
	std::string _prop;
};


class OSP_API QLRelExpr: public QLExpr
{
public:
	[[nodiscard]] bool evaluate(const Properties& props) const override;

protected:
	[[nodiscard]] virtual bool evaluateImpl(const Properties& props) const = 0;
};


class OSP_API QLEqExpr: public QLRelExpr
{
public:
	QLEqExpr(const std::string& prop, const Poco::Any& value);
	~QLEqExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLNeExpr: public QLRelExpr
{
public:
	QLNeExpr(const std::string& prop, const Poco::Any& value);
	~QLNeExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLLtExpr: public QLRelExpr
{
public:
	QLLtExpr(const std::string& prop, const Poco::Any& value);
	~QLLtExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLLeExpr: public QLRelExpr
{
public:
	QLLeExpr(const std::string& prop, const Poco::Any& value);
	~QLLeExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLGtExpr: public QLRelExpr
{
public:
	QLGtExpr(const std::string& prop, const Poco::Any& value);
	~QLGtExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLGeExpr: public QLRelExpr
{
public:
	QLGeExpr(const std::string& prop, const Poco::Any& value);
	~QLGeExpr() override;

protected:
	[[nodiscard]] bool evaluateImpl(const Properties& props) const override;

private:
	std::string _prop;
	Poco::Any   _value;
};


class OSP_API QLMatchExpr: public QLExpr
{
public:
	QLMatchExpr(const std::string& prop, const std::string& expr);
		/// Creates the QLMatchExpr for the given Poco::Glob pattern.
		///
		/// Throws a Poco::SyntaxException if the pattern contains
		/// more than MAX_WILDCARDS '*' characters.

	~QLMatchExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

	static constexpr int MAX_WILDCARDS = 16;
		/// The maximum number of '*' characters in a pattern.

private:
	std::string _prop;
	mutable Poco::Glob _glob;
		// Poco::Glob::match() is not const, but does not change the Glob.
};


class OSP_API QLMatchRegExpr: public QLExpr
{
public:
	QLMatchRegExpr(const std::string& prop, const std::string& expr);
	~QLMatchRegExpr() override;

	[[nodiscard]] bool evaluate(const Properties& props) const override;

private:
	std::string _prop;
	Poco::RegularExpression _expr;
};


class OSP_API QLTrueExpr: public QLExpr
{
public:
	QLTrueExpr();

	[[nodiscard]] bool evaluate(const Properties& props) const override;
};


} // namespace Poco::OSP


#endif // OSP_QLExpr_INCLUDED
