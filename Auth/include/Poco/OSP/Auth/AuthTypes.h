//
// AuthTypes.h
//
// Library: OSP
// Package: Auth
// Module:  AuthTypes
//
// Common type definitions for the Auth module.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_AuthTypes_INCLUDED
#define OSP_Auth_AuthTypes_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include <string>
#include <set>
#include <map>
#include <vector>


namespace Poco::OSP::Auth {


using ValueMap = std::map<std::string, std::string>;
	/// A map of names to values.

using ValueSet = std::set<std::string>;
	/// A set of values.

using Roles = ValueSet;
	/// A set of role names.

using Permissions = ValueSet;
	/// A set of permission identifiers.

using Attributes = ValueMap;
	/// A map of attribute name-value pairs.

using AttributesNameMap = std::vector<std::pair<std::string, std::string>>;
	/// A vector of attribute name pairs (LDAP attribute name, local attribute name).


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_AuthTypes_INCLUDED
