//
// Version.cpp
//
// Library: OSP
// Package: Core
// Module:  Version
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Version.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Exception.h"
#include "Poco/Ascii.h"
#include <algorithm>
#include <limits>


using Poco::NumberFormatter;


namespace Poco::OSP {


Version::Version():
	_major(0),
	_minor(0),
	_revision(0)
{
}


Version::Version(const Version& version) = default;

	
Version::Version(int major, int minor, int revision):
	_major(major),
	_minor(minor),
	_revision(revision)
{
}

	
Version::Version(int major, int minor, int revision, const std::string& designation):
	_major(major),
	_minor(minor),
	_revision(revision),
	_designation(designation)
{
}


Version::Version(const std::string& version):
	_major(0),
	_minor(0),
	_revision(0)
{
	parse(version);
}


Version::~Version() = default;


Version& Version::operator = (const Version& version)
{
	Version tmp(version);
	swap(tmp);
	return *this;
}


Version& Version::operator = (const std::string& version)
{
	parse(version);
	return *this;
}


void Version::swap(Version& version) noexcept
{
	using std::swap;
	swap(_major, version._major);
	swap(_minor, version._minor);
	swap(_revision, version._revision);
	swap(_designation, version._designation);
}

	
std::string Version::toString() const
{
	std::string result;
	result += NumberFormatter::format(_major);
	result += '.';
	result += NumberFormatter::format(_minor);
	result += '.';
	result += NumberFormatter::format(_revision);
	if (!_designation.empty())
	{
		result += '-';
		result += _designation;
	}
	return result;
}

	
namespace
{
	// Parses one number of a version. Fails if there is no digit, or
	// if the number does not fit into an int.
	[[nodiscard]] bool parseNumber(std::string::const_iterator& it, const std::string::const_iterator& end, int& number)
	{
		if (it == end || !Poco::Ascii::isDigit(*it)) return false;

		constexpr int DECIMAL_BASE = 10;
		number = 0;
		while (it != end && Poco::Ascii::isDigit(*it))
		{
			const int digit = *it++ - '0';
			if (number > (std::numeric_limits<int>::max() - digit)/DECIMAL_BASE) return false;
			number = DECIMAL_BASE*number + digit;
		}
		return true;
	}
}


void Version::parse(const std::string& str)
{
	int maj(0);
	int min(0);
	int rev(0);
	std::string designation;
	
	std::string::const_iterator it(str.begin());
	std::string::const_iterator end(str.end());
	if (parseNumber(it, end, maj))
	{
		if (it != end && *it == '.')
		{
			++it;
			if (!parseNumber(it, end, min)) throw Poco::SyntaxException("Not a valid version number", str);

			if (it != end && *it == '.')
			{
				++it;
				if (!parseNumber(it, end, rev)) throw Poco::SyntaxException("Not a valid version number", str);

				if (it != end && *it == '-')
				{
					++it;
					while (it != end)
						designation += *it++;
				}
			}
		}
	}
	else throw Poco::SyntaxException("Not a valid version number", str);

	if (it == end) 
	{
		_major = maj;
		_minor = min;
		_revision = rev;
		std::swap(_designation, designation);
	}
	else throw Poco::SyntaxException("Not a valid version number", str);
}


bool Version::operator == (const Version& version) const
{
	return _major    == version._major
	    && _minor    == version._minor
	    && _revision == version._revision;
}


bool Version::operator != (const Version& version) const
{
	return _major    != version._major
	    || _minor    != version._minor
	    || _revision != version._revision;

}


bool Version::operator <  (const Version& version) const
{
	if (_major < version._major)
	{
		return true;
	}
	else if (_major == version._major)
	{
		if (_minor < version._minor)
			return true;
		else if (_minor == version._minor)
			return _revision < version._revision;
	}
	return false;
}


bool Version::operator <= (const Version& version) const
{
	if (_major < version._major)
	{
		return true;
	}
	else if (_major == version._major)
	{
		if (_minor < version._minor)
			return true;
		else if (_minor == version._minor)
			return _revision <= version._revision;
	}
	return false;
}


bool Version::operator >  (const Version& version) const
{
	if (_major > version._major)
	{
		return true;
	}
	else if (_major == version._major)
	{
		if (_minor > version._minor)
			return true;
		else if (_minor == version._minor)
			return _revision > version._revision;
	}
	return false;
}


bool Version::operator >= (const Version& version) const
{
	if (_major > version._major)
	{
		return true;
	}
	else if (_major == version._major)
	{
		if (_minor > version._minor)
			return true;
		else if (_minor == version._minor)
			return _revision >= version._revision;
	}
	return false;
}


} // namespace Poco::OSP
