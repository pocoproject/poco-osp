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
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/ExtensionPoint.h"


namespace Poco::OSP {


ExtensionPoint::ExtensionPoint() = default;


ExtensionPoint::~ExtensionPoint() = default;


void ExtensionPoint::removeExtension(Bundle::ConstPtr /*pBundle*/, Poco::XML::Element* /*pExtensionElem*/)
{
}


} // namespace Poco::OSP
