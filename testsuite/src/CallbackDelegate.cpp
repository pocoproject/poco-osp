//
// CallbackDelegate.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "CallbackDelegate.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/AbstractDelegate.h"
#include <memory>
#include <utility>


CallbackDelegate::CallbackDelegate(Callback callback):
	_callback(std::move(callback)),
	_pKey(std::make_shared<int>(0))
{
}


CallbackDelegate::CallbackDelegate(const CallbackDelegate& other) = default;


CallbackDelegate::~CallbackDelegate() = default;


bool CallbackDelegate::notify(const void* sender, Poco::OSP::BundleEvent& event)
{
	_callback(event);
	return true;
}


Poco::AbstractDelegate<Poco::OSP::BundleEvent>* CallbackDelegate::clone() const
{
	return new CallbackDelegate(*this);
}


void CallbackDelegate::disable()
{
}


bool CallbackDelegate::equals(const Poco::AbstractDelegate<Poco::OSP::BundleEvent>& other) const
{
	const CallbackDelegate* pOther = dynamic_cast<const CallbackDelegate*>(other.unwrap());
	return pOther != nullptr && pOther->_pKey == _pKey;
}
