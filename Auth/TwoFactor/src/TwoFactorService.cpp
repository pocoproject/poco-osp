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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
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
