//
// QLParser.h
//
// Library: OSP
// Package: Util
// Module:  QLParser
//
// Definition of the QLParser class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_QLParser_INCLUDED
#define OSP_QLParser_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/QLExpr.h"
#include "Poco/StreamTokenizer.h"
#include <sstream>


namespace Poco::OSP {


class OSP_API QLParser
	/// QLParser implements a parser for property
	/// query expressions, such as used by
	/// the ServiceRegistry.
{
public:
	QLParser(const std::string& expr);
		/// Creates the QLParser for parsing the given
		/// expression.

	~QLParser();
		/// Destroys the QLParser.

	QLParser() = delete;
	QLParser(const QLParser&) = delete;
	QLParser& operator = (const QLParser&) = delete;
		
	[[nodiscard]] QLExpr::Ptr parse();
		/// Parses the expression.
		///
		/// Throws a Poco::SyntaxException if the expression
		/// is not valid, if it nests more than MAX_NESTING_DEPTH
		/// subexpressions in parentheses, or if it contains more than
		/// MAX_LOGICAL_OPERATORS "&&" and "||" operators. An empty
		/// expression matches everything.
		///
		/// A stringLiteral after "=~" is a Poco::Glob pattern with
		/// at most QLMatchExpr::MAX_WILDCARDS '*' characters; more
		/// are a syntax error.
		///
		/// A regExpr is matched with the match limit of PCRE; a match
		/// that reaches it throws a Poco::RegularExpressionException.
		///
		/// The language syntax is as follows:
		///     expr          ::= andExpr ("||" andExpr)*
		///     andExpr       ::= relExpr ("&&" relExpr)*
		///     relExpr       ::= ["!"] (id [relOp value | "=~" matchExpr]) | subExpr
		///     subExpr       ::= "(" expr ")"
		///     relOp         ::= "==" | "!=" | "<" | "<=" | ">" | ">="
		///     value         ::= numLiteral | boolLiteral | stringLiteral
		///     numLiteral    ::= (digit+ ["." digit*] | "." digit+) [("E" | "e") ["+" | "-"] digit+]
		///     boolLiteral   ::= "true" | "false"
		///     stringLiteral ::= """ char* """
		///     matchExpr     ::= stringLiteral | regExpr
		///     regExpr       ::= delim char+ delim /* valid Perl regular expression,
		///                                            enclosed in delim */
		///     delim         ::= "/" | "#"	

	static constexpr int MAX_NESTING_DEPTH = 100;
		/// The maximum nesting depth of subexpressions in parentheses.

	static constexpr int MAX_LOGICAL_OPERATORS = 1000;
		/// The maximum number of "&&" and "||" operators in an expression.

protected:
	const Poco::Token* parseExpr(const Poco::Token* next, QLExpr::Ptr& pExpr);
	const Poco::Token* parseAndExpr(const Poco::Token* next, QLExpr::Ptr& pExpr);
	const Poco::Token* parseRelExpr(const Poco::Token* next, QLExpr::Ptr& pExpr);
	void countLogicalOperator();
		/// Counts an "&&" or "||" operator. Throws a Poco::SyntaxException
		/// if there are more than MAX_LOGICAL_OPERATORS.
	[[nodiscard]] bool isRelOp(const Poco::Token* token);
	[[nodiscard]] Poco::Any token2value(const Poco::Token* token);

private:
	
	std::istringstream    _istr;
	Poco::StreamTokenizer _tokenizer;
	int                   _depth = 0;
	int                   _logicalOperators = 0;
};


} // namespace Poco::OSP


#endif // OSP_QLParser_INCLUDED
