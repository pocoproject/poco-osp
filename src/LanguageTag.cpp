//
// LanguageTag.cpp
//
// Library: OSP
// Package: Util
// Module:  LanguageTag
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/LanguageTag.h"
#include "Poco/Environment.h"
#include "Poco/Ascii.h"
#if defined(_WIN32)
#if defined(POCO_WIN32_UTF8)
#include "Poco/UnicodeConverter.h"
#endif
#include "Poco/UnWindows.h"
#endif


namespace Poco::OSP {


LanguageTag::LanguageTag()
{
	init();
}


LanguageTag::LanguageTag(const std::string& tag)
{
	init(tag);
}

	
LanguageTag::LanguageTag(const std::string& primaryTag, const std::string& subTags):
	_primaryTag(primaryTag),
	_subTags(subTags)
{
}


LanguageTag::~LanguageTag() = default;


std::string LanguageTag::toString() const
{
	std::string result(_primaryTag);
	if (!_subTags.empty())
	{
		result += '-';
		result += _subTags;
	}
	return result;
}


void LanguageTag::init()
{
#if defined(_WIN32)
#if defined(POCO_WIN32_UTF8)
	wchar_t buffer[8];
	int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buffer, sizeof(buffer)/sizeof(wchar_t));
	if (len > 0) Poco::UnicodeConverter::toUTF8(buffer, _primaryTag);
	len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buffer, sizeof(buffer)/sizeof(wchar_t));
	if (len > 0) Poco::UnicodeConverter::toUTF8(buffer, _subTags);
#else
	char buffer[8];
	int len = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buffer, sizeof(buffer));
	if (len > 0) _primaryTag.assign(buffer);
	len = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buffer, sizeof(buffer));
	if (len > 0) _subTags.assign(buffer);
#endif
#endif

	if (_primaryTag.empty())
	{
		init(Poco::Environment::get("LANG", ""));
	}
	// The C and POSIX locales name no language.
	if (_primaryTag.empty() || _primaryTag == "c" || _primaryTag == "posix")
	{
		_primaryTag = "en";
		_subTags = "US";
	}
}


void LanguageTag::init(const std::string& tag)
{
	std::string::const_iterator it = tag.begin();
	std::string::const_iterator end = tag.end();
	
	// A locale name such as "de_AT.UTF-8@euro" ends with an encoding and a modifier.
	while (it != end && *it != '-' && *it != '_' && *it != '.' && *it != '@')
	{
		_primaryTag += static_cast<char>(Poco::Ascii::toLower(*it++));
	}
	if (it != end && (*it == '-' || *it == '_')) ++it;
	while (it != end && *it != '-' && *it != '_' && *it != '.' && *it != '@')
	{
		_subTags += static_cast<char>(Poco::Ascii::toUpper(*it++));
	}
}


} // namespace Poco::OSP
