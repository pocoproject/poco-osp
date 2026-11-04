//
// BundleEvent.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleEvent
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleEvent.h"
#include <memory>
#include <utility>


namespace Poco {
namespace OSP {


BundleEvent::BundleEvent(Bundle::Ptr pBundle, EventKind what):
	_pBundle(pBundle),
	_what(what)
{
}


BundleEvent::BundleEvent(Bundle* pBundle, EventKind what):
	_pBundle(pBundle, true),
	_what(what)
{
}


BundleEvent::BundleEvent(Bundle* pBundle, const Poco::Exception& exception):
	_pBundle(pBundle, true),
	_what(EV_BUNDLE_FAILED),
	_pException(exception.clone())
{
}


BundleEvent::BundleEvent(const BundleEvent& event):
	_pBundle(event._pBundle),
	_what(event._what),
	_pException(event._pException != nullptr ? event._pException->clone() : nullptr)
{
}


BundleEvent::~BundleEvent() = default;


BundleEvent& BundleEvent::operator = (const BundleEvent& event)
{
	if (this != &event)
	{
		std::unique_ptr<Poco::Exception> pException(event._pException != nullptr ? event._pException->clone() : nullptr);
		_pBundle    = event._pBundle;
		_what       = event._what;
		_pException = std::move(pException);
	}
	return *this;
}


} } // namespace Poco::OSP
