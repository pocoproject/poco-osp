//
// PreferencesEvent.h
//
// Library: OSP
// Package: PreferencesService
// Module:  PreferencesEvent
//
// Definition of the PreferencesEvent class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_PreferencesEvent_INCLUDED
#define OSP_PreferencesEvent_INCLUDED


#include "Poco/OSP/OSP.h"


namespace Poco::OSP {


class OSP_API PreferencesEvent
	/// The PreferencesEvent class holds information about
	/// a changed property in a Preferences object.
{
public:
	PreferencesEvent(const std::string& name, const std::string& oldValue, const std::string& newValue);
		/// Creates the PreferencesEvent.

	PreferencesEvent(const PreferencesEvent& event);
		/// Creates the PreferencesEvent by copying another one.

	~PreferencesEvent();
		/// Destroys the PreferencesEvent.

	PreferencesEvent() = delete;

	PreferencesEvent& operator = (const PreferencesEvent& event);
		/// Assignment operator.
		
	void swap(PreferencesEvent& event) noexcept;
		/// Swaps the event's state with another one.

	[[nodiscard]] const std::string& name() const;
		/// Returns the name of the changed property.
		
	[[nodiscard]] const std::string& oldValue() const;
		/// Returns the value of the property before the change.
		
	[[nodiscard]] const std::string& newValue() const;
		/// Returns the value of the property after the change.

private:

	std::string _name;
	std::string _oldValue;
	std::string _newValue;
};


//
// inlines
//
inline const std::string& PreferencesEvent::name() const
{
	return _name;
}


inline const std::string& PreferencesEvent::oldValue() const
{
	return _oldValue;
}


inline const std::string& PreferencesEvent::newValue() const
{
	return _newValue;
}


} // namespace Poco::OSP


#endif // OSP_PreferencesEvent_INCLUDED
