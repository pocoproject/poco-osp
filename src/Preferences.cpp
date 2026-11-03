//
// Preferences.cpp
//
// Library: OSP
// Package: PreferencesService
// Module:  Preferences
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Preferences.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Logger.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Exception.h"


using Poco::FastMutex;


namespace Poco::OSP {


Preferences::Preferences(const std::string& path):
	_path(path),
	_pConfig(new Poco::Util::PropertyFileConfiguration()),
	_dirty(false)
{
	Poco::Path p(path);
	Poco::Path pp(p.parent());
	File pf(pp);
	if (!pf.exists())
	{
		pf.createDirectories();
	}
	File f(p);
	if (f.exists())
	{
		_pConfig->load(_path);
	}
}


Preferences::~Preferences()
{
	try
	{
		try
		{
			save();
		}
		catch (Poco::Exception& exc)
		{
			Logger::get(PreferencesService::SERVICE_NAME).error(std::string("Failed to save preferences: ")
				+ exc.displayText());
		}
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void Preferences::save()
{
	Poco::FastMutex::ScopedLock lock(_mutex);
	
	if (_dirty)
	{
		// The preferences are written to a temporary file that replaces the
		// preferences file when complete, so a failed save keeps the previous ones.
		File tmpFile(_path + ".tmp");
		try
		{
			Poco::FileOutputStream ostr(tmpFile.path());
			_pConfig->save(ostr);
			ostr.close();
			if (!ostr.good()) throw WriteFileException(tmpFile.path());
			tmpFile.renameTo(_path);
		}
		catch (...)
		{
			try
			{
				if (tmpFile.exists()) tmpFile.remove();
			}
			catch (...)
			{
				// the exception being handled is reported, not the failed cleanup
			}
			throw;
		}
		_dirty = false;
	}
}


bool Preferences::getRaw(const std::string& key, std::string& value) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_pConfig->hasProperty(key))
	{
		value = _pConfig->getRawString(key);
		return true;
	}
	else return false;
}


void Preferences::setRaw(const std::string& key, const std::string& value)		
{
	std::string oldValue;
	{
		Poco::FastMutex::ScopedLock lock(_mutex);

		if (_pConfig->hasProperty(key))
			oldValue = _pConfig->getRawString(key);
	}

	// Fired without the lock, so that a delegate can read the preferences.
	PreferencesEvent propertyChangedEvent(key, oldValue, value);
	propertyChanged(this, propertyChangedEvent);

	Poco::FastMutex::ScopedLock lock(_mutex);

	_pConfig->setString(key, value);
	_dirty = true;
}


void Preferences::enumerate(const std::string& key, Keys& range) const
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	return _pConfig->keys(key, range);
}


void Preferences::removeRaw(const std::string& key)
{
	Poco::FastMutex::ScopedLock lock(_mutex);

	_pConfig->remove(key);
	_dirty = true;
}


} // namespace Poco::OSP
