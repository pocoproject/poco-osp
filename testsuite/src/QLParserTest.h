//
// QLParserTest.h
//
// Definition of the QLParserTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef QLParserTest_INCLUDED
#define QLParserTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class QLParserTest: public CppUnit::TestCase
{
public:
	QLParserTest(const std::string& name);
	~QLParserTest() override;

	void testPropertyPresence();
	void testStringComparison();
	void testNumericAndBooleanComparison();
	void testBooleanOperatorsAndPrecedence();
	void testGlobMatchOfWholeValue();
	void testRegexMatch();
	void testMalformedQueryIsSyntaxError();
	void testInvalidRegexIsRejected();
	void testMissingPropertyComparesAsEmptyString();
	void testNumberComparedWithStringComparesText();
	void testEscapedQuoteInString();
	void testMissingOperandIsSyntaxError();
	void testDeepNestingIsRejected();
	void testLongOperatorChainIsRejected();
	void testGlobMatch();
	void testGlobPatternWithTooManyWildcardsIsSyntaxError();
	void testIncompleteTokenAtEndIsReportedAsEndOfExpression();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
};


#endif // QLParserTest_INCLUDED
