//
// QLParserTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "QLParserTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/QLExpr.h"
#include "Poco/OSP/QLParser.h"
#include "Poco/Exception.h"
#include <vector>


using Poco::OSP::Properties;
using Poco::OSP::QLExpr;
using Poco::OSP::QLMatchExpr;
using Poco::OSP::QLParser;


namespace
{
	QLExpr::Ptr parse(const std::string& expr)
	{
		QLParser parser(expr);
		return parser.parse();
	}


	[[nodiscard]] Properties sampleProperties()
		/// Returns a string, a number, a fraction and two booleans.
	{
		Properties props;
		props.set("s", "foo");
		props.set("n", "42");
		props.set("x", "0.5");
		props.set("t", "true");
		props.set("f", "false");
		return props;
	}
}


QLParserTest::QLParserTest(const std::string& name): CppUnit::TestCase(name)
{
}


QLParserTest::~QLParserTest()
{
}


void QLParserTest::testPropertyPresence()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("s");
	assert (pExpr->evaluate(props));

	pExpr = parse("z");
	assert (!pExpr->evaluate(props));

	pExpr = parse("!z");
	assert (pExpr->evaluate(props));
}


void QLParserTest::testStringComparison()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("s == \"foo\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"bar\"");
	assert (!pExpr->evaluate(props));
	
	pExpr = parse("s != \"bar\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s != \"foo\"");
	assert (!pExpr->evaluate(props));
	
	pExpr = parse("s < \"ggg\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s <= \"ggg\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s < \"aaa\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s <= \"aaa\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s < \"foo\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s <= \"foo\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s > \"eee\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s >= \"eee\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s > \"ggg\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s >= \"ggg\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s > \"foo\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s >= \"foo\"");
	assert (pExpr->evaluate(props));
}


void QLParserTest::testNumericAndBooleanComparison()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("n == 42");
	assert (pExpr->evaluate(props));

	pExpr = parse("n != 24");
	assert (pExpr->evaluate(props));

	pExpr = parse("n != 42");
	assert (!pExpr->evaluate(props));

	pExpr = parse("n == 24");
	assert (!pExpr->evaluate(props));
	
	pExpr = parse("n < 100");
	assert (pExpr->evaluate(props));

	pExpr = parse("n > 10");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("t == true");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("f == false");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("x == 0.5");
	assert (pExpr->evaluate(props));

	pExpr = parse("x != 1.5");
	assert (pExpr->evaluate(props));
}


void QLParserTest::testBooleanOperatorsAndPrecedence()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("s == \"foo\" && n == 42");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s == \"foo\" && n == 24");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" && n == 42");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s == \"foo\" && n == 42 && t == true");
	assert (pExpr->evaluate(props));

	pExpr = parse("s && n && t");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"foo\" || n == 42");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s == \"foo\" || n == 24");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" || n == 42");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" || n == 24");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" || n == 24 || t == true");
	assert (pExpr->evaluate(props));

	pExpr = parse("s || n || t");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s == \"bar\" && n == 42 || t == true");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s == \"foo\" && n == 24 || t == true");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" || n == 24 && t == true");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s == \"bar\" || n == 42 && t == true");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("!s == \"bar\"");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s == \"foo\" && (n == 24 || t)");
	assert (pExpr->evaluate(props));

	pExpr = parse("s == \"foo\" && (n == 42 && (t || z))");
	assert (pExpr->evaluate(props));
}


void QLParserTest::testGlobMatchOfWholeValue()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("s =~ \"f*\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s =~ \"f*z\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s =~ \"f??\"");
	assert (pExpr->evaluate(props));
	
	pExpr = parse("s =~ \"f?z\"");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s =~ \"foo\"");
	assert (pExpr->evaluate(props));

	pExpr = parse("s =~ \"bar\"");
	assert (!pExpr->evaluate(props));
}


void QLParserTest::testRegexMatch()
{
	const Properties props(sampleProperties());

	QLExpr::Ptr pExpr = parse("s =~ /foo/");
	assert (pExpr->evaluate(props));

	pExpr = parse("s =~ /.*/");
	assert (pExpr->evaluate(props));

	pExpr = parse("s =~ /f../");
	assert (pExpr->evaluate(props));

	pExpr = parse("s =~ /oo/");
	assert (!pExpr->evaluate(props));

	pExpr = parse("s =~ /.oo/");
	assert (pExpr->evaluate(props));
}


void QLParserTest::testMalformedQueryIsSyntaxError()
{
	const std::vector<std::string> queries = {"(", "(s", "s ==", "s == \"foo", "s =~ /foo", "s t", "s == \"a\" \"b\"", "== s"};
	for (const auto& query: queries)
	{
		try
		{
			parse(query);
			failmsg("query '" + query + "' must be rejected");
		}
		catch (Poco::SyntaxException&)
		{
		}
	}
}


void QLParserTest::testInvalidRegexIsRejected()
{
	try
	{
		parse("s =~ /(/");
		fail("invalid regular expression - must throw");
	}
	catch (Poco::RegularExpressionException&)
	{
	}
}


void QLParserTest::testMissingPropertyComparesAsEmptyString()
{
	const Properties props(sampleProperties());
	assertFalse (parse("z == \"foo\"")->evaluate(props));
	assertTrue (parse("z != \"foo\"")->evaluate(props));
	assertTrue (parse("z == \"\"")->evaluate(props));
	assertTrue (parse("z =~ \"*\"")->evaluate(props));
	assertFalse (parse("z =~ \"?*\"")->evaluate(props));
	assertFalse (parse("z == 42")->evaluate(props));
}


void QLParserTest::testNumberComparedWithStringComparesText()
{
	// "42" < "abc" as text
	const Properties props(sampleProperties());
	assertTrue (parse("n < \"abc\"")->evaluate(props));
	assertTrue (parse("n == \"42\"")->evaluate(props));
	assertFalse (parse("s == 42")->evaluate(props));
}


void QLParserTest::testEscapedQuoteInString()
{
	Properties props;
	props.set("q", "a\"b");
	assertTrue (parse("q == \"a\\\"b\"")->evaluate(props));
}


void QLParserTest::testMissingOperandIsSyntaxError()
{
	const std::vector<std::string> queries = {"&& s", "s &&", "s ||", "!", "()", "() && s"};
	for (const auto& query: queries)
	{
		try
		{
			parse(query);
			failmsg("query '" + query + "' must be rejected");
		}
		catch (Poco::SyntaxException&)
		{
		}
	}

	Properties props;
	assertTrue (parse("")->evaluate(props));
}


void QLParserTest::testDeepNestingIsRejected()
{
	const std::string::size_type depth = 1000;
	const std::string query(std::string(depth, '(') + "s" + std::string(depth, ')'));
	try
	{
		parse(query);
		fail("nesting beyond the limit must be rejected");
	}
	catch (Poco::SyntaxException&)
	{
	}

	const std::string::size_type maxDepth = QLParser::MAX_NESTING_DEPTH;
	Properties props;
	props.set("s", "foo");
	assertTrue (parse(std::string(maxDepth, '(') + "s" + std::string(maxDepth, ')'))->evaluate(props));
}


void QLParserTest::testLongOperatorChainIsRejected()
{
	const std::string::size_type maxOperators = QLParser::MAX_LOGICAL_OPERATORS;
	std::string query("s");
	for (std::string::size_type i = 0; i < maxOperators; ++i)
	{
		query += (i % 2 == 0) ? " && s" : " || s";
	}
	Properties props;
	props.set("s", "foo");
	assertTrue (parse(query)->evaluate(props));

	query += " && s";
	try
	{
		parse(query);
		fail("a chain of more operators than the limit must be rejected");
	}
	catch (Poco::SyntaxException&)
	{
	}
}


void QLParserTest::testGlobMatch()
{
	Properties props;
	props.set("s", "b-\xC3\xA4");

	assertTrue (parse("s =~ \"[a-c]-?\"")->evaluate(props));
	assertTrue (parse("s =~ \"b*\"")->evaluate(props));
	assertFalse (parse("s =~ \"[!a-c]*\"")->evaluate(props));
	assertFalse (parse("s =~ \"b-??\"")->evaluate(props));
}


void QLParserTest::testGlobPatternWithTooManyWildcardsIsSyntaxError()
{
	const std::string::size_type maxWildcards = QLMatchExpr::MAX_WILDCARDS;
	try
	{
		parse("s =~ \"" + std::string(maxWildcards + 1, '*') + "\"");
		fail("pattern with more wildcards than the limit must be rejected");
	}
	catch (Poco::SyntaxException&)
	{
	}

	Properties props;
	props.set("s", "foo");
	assertTrue (parse("s =~ \"" + std::string(maxWildcards, '*') + "\"")->evaluate(props));
}


void QLParserTest::testIncompleteTokenAtEndIsReportedAsEndOfExpression()
{
	// The message is checked: it must not show eof() as a character.
	const std::vector<std::string> queries = {"s &", "s |", "s == 1e"};
	for (const auto& query: queries)
	{
		try
		{
			parse(query);
			failmsg("query '" + query + "' must be rejected");
		}
		catch (Poco::SyntaxException& exc)
		{
			assertTrue (exc.message().find("got: end of expression") != std::string::npos);
		}
	}
}


void QLParserTest::setUp()
{
}


void QLParserTest::tearDown()
{
}


CppUnit::Test* QLParserTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("QLParserTest");

	CppUnit_addTest(pSuite, QLParserTest, testPropertyPresence);
	CppUnit_addTest(pSuite, QLParserTest, testStringComparison);
	CppUnit_addTest(pSuite, QLParserTest, testNumericAndBooleanComparison);
	CppUnit_addTest(pSuite, QLParserTest, testBooleanOperatorsAndPrecedence);
	CppUnit_addTest(pSuite, QLParserTest, testGlobMatchOfWholeValue);
	CppUnit_addTest(pSuite, QLParserTest, testRegexMatch);
	CppUnit_addTest(pSuite, QLParserTest, testMalformedQueryIsSyntaxError);
	CppUnit_addTest(pSuite, QLParserTest, testInvalidRegexIsRejected);
	CppUnit_addTest(pSuite, QLParserTest, testMissingPropertyComparesAsEmptyString);
	CppUnit_addTest(pSuite, QLParserTest, testNumberComparedWithStringComparesText);
	CppUnit_addTest(pSuite, QLParserTest, testEscapedQuoteInString);
	CppUnit_addTest(pSuite, QLParserTest, testMissingOperandIsSyntaxError);
	CppUnit_addTest(pSuite, QLParserTest, testDeepNestingIsRejected);
	CppUnit_addTest(pSuite, QLParserTest, testLongOperatorChainIsRejected);
	CppUnit_addTest(pSuite, QLParserTest, testGlobMatch);
	CppUnit_addTest(pSuite, QLParserTest, testGlobPatternWithTooManyWildcardsIsSyntaxError);
	CppUnit_addTest(pSuite, QLParserTest, testIncompleteTokenAtEndIsReportedAsEndOfExpression);

	return pSuite;
}
