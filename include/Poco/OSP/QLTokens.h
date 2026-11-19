//
// QLTokens.h
//
// Library: OSP
// Package: Util
// Module:  QLTokens
//
// Definition of the QLTokens class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_QLTokens_INCLUDED
#define OSP_QLTokens_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/Token.h"
#include <map>


namespace Poco::OSP {


class OSP_API QLToken: public Poco::Token
	/// The base class for all query language tokens.
{
public:
	QLToken();
	~QLToken() override;

protected:
	[[noreturn]] void syntaxError(const std::string& expected, const std::string& actual);

	[[nodiscard]] static std::string characterText(int ch);
		/// Returns ch, a result of std::istream::peek(), as text
		/// for an error message: the character itself, or "end of
		/// expression" for std::char_traits<char>::eof().
};


class OSP_API OperatorToken: public QLToken
{
public:
	enum Tokens
	{
		OP_OPENPARENT = 1,  // (
		OP_CLOSPARENT,      // )
		OP_LT,              // <
		OP_LE,              // <=
		OP_GT,              // >
		OP_GE,              // >=
		OP_EQ,              // ==
		OP_MATCH,           // =~
		OP_NOT,             // !
		OP_NE,              // !=
		OP_AND,             // &&
		OP_OR               // ||
	};

	OperatorToken();
	~OperatorToken() override;
	[[nodiscard]] Poco::Token::Class tokenClass() const override;
	bool start(char c, std::istream& istr) override;
	void finish(std::istream& istr) override;
	[[nodiscard]] int asInteger() const override;

private:
	using OpMap = std::map<std::string, int>;

	OpMap _opMap;
};


class OSP_API IdentifierToken: public QLToken
{
public:
	IdentifierToken();
	~IdentifierToken() override;
	[[nodiscard]] Poco::Token::Class tokenClass() const override;
	bool start(char c, std::istream& istr) override;
	void finish(std::istream& istr) override;
};


class OSP_API StringLiteralToken: public QLToken
{
public:
	StringLiteralToken();
	~StringLiteralToken() override;
	[[nodiscard]] Poco::Token::Class tokenClass() const override;
	bool start(char c, std::istream& istr) override;
	void finish(std::istream& istr) override;
	[[nodiscard]] std::string asString() const override;
};


class OSP_API RegExpToken: public QLToken
{
public:
	RegExpToken();
	~RegExpToken() override;
	[[nodiscard]] Poco::Token::Class tokenClass() const override;
	bool start(char c, std::istream& istr) override;
	void finish(std::istream& istr) override;
	[[nodiscard]] std::string asString() const override;
};


class OSP_API NumberLiteralToken: public QLToken
{
public:
	NumberLiteralToken();
	~NumberLiteralToken() override;
	[[nodiscard]] Poco::Token::Class tokenClass() const override;
	bool start(char c, std::istream& istr) override;
	void finish(std::istream& istr) override;

private:
	bool _isFloat = false;
};


} // namespace Poco::OSP


#endif // OSP_QLTokens_INCLUDED
