//
// MediaTypeMapper.h
//
// Library: OSP/Web
// Package: Web
// Module:  MediaTypeMapper
//
// Definition of the MediaTypeMapper class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Web_MediaTypeMapper_INCLUDED
#define OSP_Web_MediaTypeMapper_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Service.h"
#include "Poco/AutoPtr.h"
#include "Poco/Mutex.h"
#include <map>
#include <istream>


namespace Poco::OSP::Web {


class OSPWeb_API MediaTypeMapper: public Service
	/// The MediaTypeMapper service maps file extensions to MIME media types.
	///
	/// For more information on MIME media types, please see RFC 2045 and RFC 2046.
	///
	/// The MediaTypeMapper can load extension to media type associations from
	/// a configuration file. The configuration file has the same format as the
	/// Apache mime.types file, used by the Apache mod_mime module (see
	/// http://httpd.apache.org/docs/1.3/mod/mod_mime.html).
	///
	/// The mime.types file consists of lines, each containing an association
	/// between a media type and zero or more extensions. Media type and
	/// extensions are delimited by whitespace. Lines starting with a hash
	/// character ('#') are treated as comments and ignored.
	///
	/// The name of the MediaTypeMapper service is "osp.web.mediatype".
	///
	/// add(), map(), setDefault() and getDefault() can be called from several
	/// threads. find(), begin() and end() return iterators into the mappings
	/// and must not be used while another thread calls add() or load().
{
public:
	using Ptr = Poco::AutoPtr<MediaTypeMapper>;
	using SuffixToMediaTypeMap = std::map<std::string, std::string>;
	using ConstIterator = SuffixToMediaTypeMap::const_iterator;

	MediaTypeMapper();
		/// Creates an empty MediaTypeMapper.

	~MediaTypeMapper() override;
		/// Destroys the MediaTypeMapper.

	void load(std::istream& str);
		/// Loads mappings from a stream or file conforming to the
		/// Apache mime.types file format. The mappings from the file
		/// are added to the existing mappings.

	void add(const std::string& suffix, const std::string& mediaType);
		/// Adds the suffix->MIME media type mapping, or replaces the
		/// media type of an existing mapping for the suffix.
		/// Suffix is not case sensitive.

	[[nodiscard]] ConstIterator find(const std::string& suffix) const;
		/// Searches a MIME media type for the given suffix. Returns end() for unknown mappings.
		/// Note that the search is not case sensitive!

	[[nodiscard]] ConstIterator begin() const;
		/// Returns the begin iterator for the suffix to MIME media type map.

	[[nodiscard]] ConstIterator end() const;
		/// Returns the end iterator for the suffix to MIME media type map.

	[[nodiscard]] std::string map(const std::string& suffix) const;
		/// Returns a MIME media type for the given suffix. If no media type has been
		/// registered for the given suffix, the default media type
		/// (application/octet-stream, unless another one has been set) is returned.
		/// Note that the search is not case sensitive!

	void setDefault(const std::string& mediaType);
		/// Sets a default MIME media type for unknown suffixes.

	[[nodiscard]] std::string getDefault() const;
		/// Returns the default MIME media type.
		///
		/// Unless another default type has been set with setDefault(),
		/// the default type is application/octet-stream.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;

private:
	SuffixToMediaTypeMap _map;
	std::string          _default;
	mutable Poco::FastMutex _mutex;
};


//
// inlines
//
inline MediaTypeMapper::ConstIterator MediaTypeMapper::begin() const
{
	return _map.begin();
}


inline MediaTypeMapper::ConstIterator MediaTypeMapper::end() const
{
	return _map.end();
}


} // namespace Poco::OSP::Web


#endif // OSP_Web_MediaTypeMapper_INCLUDED
