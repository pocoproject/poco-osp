//
// MediaTypeMapperTest.h
//
// Definition of the MediaTypeMapperTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef MediaTypeMapperTest_INCLUDED
#define MediaTypeMapperTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"


class MediaTypeMapperTest: public CppUnit::TestCase
{
public:
	MediaTypeMapperTest(const std::string& name);
	~MediaTypeMapperTest() override;

	void testLoad();
	void testMapIgnoresCase();
	void testMapReturnsDefaultType();
	void testAddReplacesMapping();
	void testMapWhileAdding();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
};


#endif // MediaTypeMapperTest_INCLUDED
