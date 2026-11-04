//
// Version.h
//
// Library: OSP
// Package: Core
// Module:  Version
//
// Definition of the Version class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Version_INCLUDED
#define OSP_Version_INCLUDED


#include "Poco/OSP/OSP.h"


// we get these stupid #defines on Mac OS X from <sys/types.h>
#undef major
#undef minor


namespace Poco::OSP {


class OSP_API Version
	/// This class represents a bundle version number
	/// in OSP. A version number consists of a
	/// major version number, a minor version number
	/// and a revision number. Optionally, a release
	/// designation can be specified.
	///
	/// A Version object can be formatted as a string.
	/// Also, a Version object can be constructed by
	/// parsing a string.
	///
	/// The string representation of a version number is
	/// <major>.<minor>.<revision>[-<designation>]
	///
	/// In a comparison, only major, minor and revision number
	/// are considered. The release designation is ignored.
{
public:
	Version();
		/// Creates a Version with all numbers set to zero.

	Version(const Version& version);
		/// Creates a Version by copying another one.

	Version(int major, int minor, int revision);
		/// Creates a Version from a major, minor and revision number.

	Version(int major, int minor, int revision, const std::string& designation);
		/// Creates a Version from a major, minor and revision number,
		/// as well as a release designation.

	explicit Version(const std::string& version);
		/// Creates a Version from its string representation.
		///
		/// Throws a Poco::SyntaxException if the string is not a valid
		/// version: each given number has at least one digit and fits
		/// into an int.

	~Version();
		/// Destroys the Version.

	Version& operator = (const Version& version);
		/// Assigns another version.

	Version& operator = (const std::string& version);
		/// Assigns a version parsed from a string.
		///
		/// Throws a Poco::SyntaxException if the string is not a valid
		/// version (see Version(const std::string&)).

	void swap(Version& version) noexcept;
		/// Swaps the version with another one.

	[[nodiscard]] int major() const;
		/// Returns the major version number.

	[[nodiscard]] int minor() const;
		/// Returns the minor version number.

	[[nodiscard]] int revision() const;
		/// Returns the revision number.

	[[nodiscard]] const std::string& designation() const;
		/// Returns the release designation.

	[[nodiscard]] std::string toString() const;
		/// Returns a string representation of the version.

	bool operator == (const Version& version) const;
	bool operator != (const Version& version) const;
	bool operator <  (const Version& version) const;
	bool operator <= (const Version& version) const;
	bool operator >  (const Version& version) const;
	bool operator >= (const Version& version) const;

protected:
	void parse(const std::string& str);
		/// Parses the version from a string.

private:
	int _major;
	int _minor;
	int _revision;
	std::string _designation;
};


//
// inlines
//
inline int Version::major() const
{
	return _major;
}


inline int Version::minor() const
{
	return _minor;
}


inline int Version::revision() const
{
	return _revision;
}


inline const std::string& Version::designation() const
{
	return _designation;
}


inline void swap(Version& v1, Version& v2) noexcept
{
	v1.swap(v2);
}


} // namespace Poco::OSP


#endif // OSP_Version_INCLUDED
