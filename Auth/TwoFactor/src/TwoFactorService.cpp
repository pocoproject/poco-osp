//
// TwoFactorService.cpp
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  TwoFactorService
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/TwoFactor/TwoFactorService.h"


namespace Poco::OSP::Auth::TwoFactor {


const std::string TwoFactorService::SERVICE_NAME("osp.auth.twofactor");


const std::type_info& TwoFactorService::type() const
{
	return typeid(TwoFactorService);
}


bool TwoFactorService::isA(const std::type_info& otherType) const
{
	return otherType == typeid(TwoFactorService);
}


} // namespace Poco::OSP::Auth::TwoFactor
