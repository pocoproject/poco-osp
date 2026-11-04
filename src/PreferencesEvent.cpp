//
// PreferencesEvent.cpp
//
// Library: OSP
// Package: PreferencesService
// Module:  PreferencesEvent
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/PreferencesEvent.h"
#include <algorithm>


namespace Poco::OSP {


PreferencesEvent::PreferencesEvent(const std::string& name, const std::string& oldValue, const std::string& newValue):
	_name(name),
	_oldValue(oldValue),
	_newValue(newValue)
{
}


PreferencesEvent::PreferencesEvent(const PreferencesEvent& event) = default;


PreferencesEvent::~PreferencesEvent() = default;


PreferencesEvent& PreferencesEvent::operator = (const PreferencesEvent& event)
{
	PreferencesEvent tmp(event);
	swap(tmp);
	return *this;
}


void PreferencesEvent::swap(PreferencesEvent& event) noexcept
{
	using std::swap;
	swap(_name, event._name);
	swap(_oldValue, event._oldValue);
	swap(_newValue, event._newValue);
}


} // namespace Poco::OSP
