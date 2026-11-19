//
// ExtensionPoint.cpp
//
// Library: OSP
// Package: ExtensionPointService
// Module:  ExtensionPoint
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/ExtensionPoint.h"


namespace Poco::OSP {


ExtensionPoint::ExtensionPoint() = default;


ExtensionPoint::~ExtensionPoint() = default;


void ExtensionPoint::removeExtension(Bundle::ConstPtr /*pBundle*/, Poco::XML::Element* /*pExtensionElem*/)
{
}


} // namespace Poco::OSP
