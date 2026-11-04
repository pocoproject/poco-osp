//
// Real.h
//
// A remote service, for the case with the real RemoteGenNG.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef Sample_Real_INCLUDED
#define Sample_Real_INCLUDED


#include <string>


namespace Sample {


//@ remote
class Real
{
public:
	std::string name() const;
};


} // namespace Sample


#endif // Sample_Real_INCLUDED
