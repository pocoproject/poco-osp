//
// QLTokens.cpp
//
// Library: OSP
// Package: Util
// Module:  QLTokens
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/QLTokens.h"
#include "Poco/Exception.h"
#include <cctype>
#include <string>


using Poco::Token;


namespace Poco::OSP {


//
// QLToken
//


QLToken::QLToken() = default;


QLToken::~QLToken() = default;


void QLToken::syntaxError(const std::string& expected, const std::string& actual)
{
	std::string msg("expected: ");
	msg.append(expected);
	msg.append(", got: ");
	msg.append(actual);
	throw Poco::SyntaxException(msg);
}


std::string QLToken::characterText(int ch)
{
	if (ch == std::char_traits<char>::eof())
		return "end of expression";
	else
		return std::string(1, static_cast<char>(ch));
}


//
// OperatorToken
//


OperatorToken::OperatorToken()
{
	int i = 1;
	_opMap["("]  = i++;
	_opMap[")"]  = i++;
	_opMap["<"]  = i++;
	_opMap["<="] = i++;
	_opMap[">"]  = i++;
	_opMap[">="] = i++;
	_opMap["=="] = i++;
	_opMap["=~"] = i++;
	_opMap["!"]  = i++;
	_opMap["!="] = i++;
	_opMap["&&"] = i++;
	_opMap["||"] = i++;
}


OperatorToken::~OperatorToken() = default;


Token::Class OperatorToken::tokenClass() const
{
	return Token::OPERATOR_TOKEN;
}


bool OperatorToken::start(char c, std::istream& /*istr*/)
{
	_value = c;
	switch (_value[0])
	{
	case '(':
	case ')':
	case '<':
	case '>':
	case '=':
	case '!':
	case '&':
	case '|':
		return true;
	default:
		return false;
	}
}


void OperatorToken::finish(std::istream& istr)
{
	const int next = istr.peek();
	switch (_value[0])
	{
	case '(':
	case ')':
		break;
	case '<':
		if (next == '=') _value += static_cast<char>(istr.get());
		break;
	case '>':
		if (next == '=') _value += static_cast<char>(istr.get());
		break;
	case '&':
		if (next == '&')
			_value += static_cast<char>(istr.get());
		else
			syntaxError("&", characterText(next));
		break;
	case '|':
		if (next == '|')
			_value += static_cast<char>(istr.get());
		else
			syntaxError("|", characterText(next));
		break;
	case '=':
		if (next == '~' || next == '=') _value += static_cast<char>(istr.get());
		break;
	case '!':
		if (next == '=') _value += static_cast<char>(istr.get());
		break;
	default:
		poco_bugcheck();
	}
}


int OperatorToken::asInteger() const
{
	OpMap::const_iterator it = _opMap.find(_value);
	if (it != _opMap.end())
		return it->second;
	else
		return 0;
}


//
// IdentifierToken
//


IdentifierToken::IdentifierToken() = default;


IdentifierToken::~IdentifierToken() = default;


Token::Class IdentifierToken::tokenClass() const
{
	return Token::IDENTIFIER_TOKEN;
}


bool IdentifierToken::start(char c, std::istream& /*istr*/)
{
	_value = c;
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       c == '_';
}


void IdentifierToken::finish(std::istream& istr)
{
	int next = istr.peek();
	while ((next >= 'A' && next <= 'Z') ||
		   (next >= 'a' && next <= 'z') ||
		   (next >= '0' && next <= '9') ||
		    next == '_' || next == '.') 
	{
		_value += static_cast<char>(istr.get());
		next = istr.peek();
	}
}


//
// StringLiteralToken
//


StringLiteralToken::StringLiteralToken() = default;


StringLiteralToken::~StringLiteralToken() = default;


Token::Class StringLiteralToken::tokenClass() const
{
	return Token::STRING_LITERAL_TOKEN;
}


bool StringLiteralToken::start(char c, std::istream& /*istr*/)
{
	_value = c;
	return c == '"';
}


void StringLiteralToken::finish(std::istream& istr)
{
	int next = istr.peek();	
	while (next != -1 && next != '"' && next != '\n' && next != '\r')
	{
		if (next == '\\') _value += static_cast<char>(istr.get());
		_value += static_cast<char>(istr.get());
		next = istr.peek();
	}
	if (next == '"')
	{
		next = istr.get();
		_value += static_cast<char>(next);
	}
	else throw SyntaxException("Unterminated string literal");
}


std::string StringLiteralToken::asString() const
{
	std::string result;
	std::string::const_iterator it  = _value.begin();
	std::string::const_iterator end = _value.end();
	if (it != end)
	{
		if (*it == '"') ++it;
		while (it != end && *it != '"')
		{
			if (*it == '\\') ++it;
			if (it != end) result += *it++;
		}
	}
	return result;
}


//
// RegExpToken
//


RegExpToken::RegExpToken() = default;


RegExpToken::~RegExpToken() = default;


Token::Class RegExpToken::tokenClass() const
{
	return Token::USER_TOKEN;
}


bool RegExpToken::start(char c, std::istream& /*istr*/)
{
	_value = c;
	return c == '/' || c == '#';
}


void RegExpToken::finish(std::istream& istr)
{
	int next = istr.peek();	
	while (next != -1 && next != _value[0] && next != '\n' && next != '\r')
	{
		_value += static_cast<char>(istr.get());
		next = istr.peek();
	}
	if (next == _value[0])
	{
		next = istr.get();
		_value += static_cast<char>(next);
	}
	else throw SyntaxException("Unterminated regular expression");
}


std::string RegExpToken::asString() const
{
	std::string result;
	std::string::const_iterator it  = _value.begin();
	std::string::const_iterator end = _value.end();
	if (it != end)
	{
		if (*it == _value[0]) ++it;
		while (it != end && *it != _value[0]) result += *it++;
	}
	return result;
}


//
// NumberLiteralToken
//


NumberLiteralToken::NumberLiteralToken() = default;


NumberLiteralToken::~NumberLiteralToken() = default;


Token::Class NumberLiteralToken::tokenClass() const
{
	return _isFloat ? Token::FLOAT_LITERAL_TOKEN : Token::INTEGER_LITERAL_TOKEN;
}


bool NumberLiteralToken::start(char c, std::istream& istr)
{
	_value = c;
	int next = istr.peek();
	return (c >= '0' && c <= '9') ||
	       (c == '.' && next >= '0' && next <= '9');
}


void NumberLiteralToken::finish(std::istream& istr)
{
	int next = istr.peek();
	_isFloat = false;
	if (_value[0] != '.') // starts with digit
	{
		if (next == 'x')
		{
			_value += static_cast<char>(istr.get());
			next = istr.peek();
			while (std::isxdigit(next)) { _value += static_cast<char>(istr.get()); next = istr.peek(); }
			return;
		}
		while (next >= '0' && next <= '9')
		{
			_value += static_cast<char>(istr.get());
			next = istr.peek();
		}
		if (next == '.')
		{
			next = istr.get();
			next = istr.peek();
			if (next != '.')
			{
				_isFloat = true;
				_value += '.';
			}
			else // double period
			{
				istr.unget();
			}
		}
	}
	else
	{
		_isFloat = true;
		_value += static_cast<char>(istr.get());
		next = istr.peek();
	}
	while (next >= '0' && next <= '9')
	{
		_value += static_cast<char>(istr.get());
		next = istr.peek();
	}
	if (next == 'e' || next == 'E')
	{
		_isFloat = true;
		_value += static_cast<char>(istr.get());
		next = istr.peek();
		if (next == '+' || next == '-')
		{
			_value += static_cast<char>(istr.get());
			next = istr.peek();
		}
		if (next >= '0' && next <= '9')
		{
			while (next >= '0' && next <= '9')
			{
				_value += static_cast<char>(istr.get());
				next = istr.peek();
			}
		}
		else
		{
			syntaxError("digit", characterText(next));
		}
	}
}


} // namespace Poco::OSP
