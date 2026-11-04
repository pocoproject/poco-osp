//
// MediaTypeMapper.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  MediaTypeMapper
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/String.h"
#include "Poco/Mutex.h"
#include <cctype>


namespace Poco {
namespace OSP {
namespace Web {


const std::string MediaTypeMapper::SERVICE_NAME("osp.web.mediatype");


MediaTypeMapper::MediaTypeMapper():
	_default("application/octet-stream")
{
}


MediaTypeMapper::~MediaTypeMapper()
{
}


void MediaTypeMapper::load(std::istream& istr)
{
	static const int eof = std::char_traits<char>::eof();

	int ch = istr.get();
	while (ch != eof)
	{
		while (std::isspace(ch)) ch = istr.get();
		if (ch == '#')
		{
			ch = istr.get();
			while (ch != eof && ch != '\n') ch = istr.get();
		}
		else
		{
			std::string mediaType;
			while (ch != eof && !std::isspace(ch))
			{
				mediaType += (char) ch;
				ch = istr.get();
			}
			while (ch != eof && ch != '\n')
			{
				while (ch != eof && std::isspace(ch) && ch != '\n') ch = istr.get();
				std::string ext;
				while (ch != eof && !std::isspace(ch))
				{
					ext += (char) ch;
					ch = istr.get();
				}
				if (!ext.empty())
					add(ext, mediaType);
			}
		}
	}
}


MediaTypeMapper::ConstIterator MediaTypeMapper::find(const std::string& suffix) const
{
	return _map.find(Poco::toLower(suffix));
}


void MediaTypeMapper::add(const std::string& suffix, const std::string& mediaType)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_map[Poco::toLower(suffix)] = mediaType;
}


std::string MediaTypeMapper::map(const std::string& suffix) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	ConstIterator it = _map.find(Poco::toLower(suffix));
	if (it != _map.end())
		return it->second;
	else
		return _default;
}


void MediaTypeMapper::setDefault(const std::string& mediaType)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_default = mediaType;
}


std::string MediaTypeMapper::getDefault() const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _default;
}


const std::type_info& MediaTypeMapper::type() const
{
	return typeid(MediaTypeMapper);
}


bool MediaTypeMapper::isA(const std::type_info& otherType) const
{
	std::string name(typeid(MediaTypeMapper).name());
	return name == otherType.name() || Service::isA(otherType);
}


} } } // namespace Poco::OSP::Web
