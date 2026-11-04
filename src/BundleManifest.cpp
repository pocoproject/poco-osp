//
// BundleManifest.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleManifest
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/Version.h"
#include "Poco/Format.h"
#include "Poco/Ascii.h"
#include "Poco/Exception.h"
#include <memory>
#include <algorithm>
#include <string>


using namespace std::string_literals;


namespace Poco {
namespace OSP {


namespace
{
	[[nodiscard]] Version parseVersion(const std::string& header, const std::string& version)
		/// Returns the version given in the header, or throws a
		/// ManifestException that names the header.
	{
		try
		{
			return Version(version);
		}
		catch (Poco::SyntaxException& exc)
		{
			throw ManifestException(header, exc.message());
		}
	}


	void skipItemEnd(const std::string& header, std::string::const_iterator& it, std::string::const_iterator end)
		/// Skips the white space following an item, and throws a
		/// ManifestException that names the header if the item is
		/// followed by anything other than ',' or the end of the header.
	{
		while (it != end && Poco::Ascii::isSpace(*it)) ++it;
		if (it != end && *it != ',') throw ManifestException(header, "Expected ',' following item");
	}
}


const std::string BundleManifest::VERSION("1.0");
const std::string BundleManifest::MANIFEST_VERSION("Manifest-Version");
const std::string BundleManifest::BUNDLE_NAME("Bundle-Name");
const std::string BundleManifest::BUNDLE_VENDOR("Bundle-Vendor");
const std::string BundleManifest::BUNDLE_COPYRIGHT("Bundle-Copyright");
const std::string BundleManifest::BUNDLE_SYMBOLICNAME("Bundle-SymbolicName");
const std::string BundleManifest::BUNDLE_VERSION("Bundle-Version");
const std::string BundleManifest::BUNDLE_ACTIVATOR("Bundle-Activator");
const std::string BundleManifest::REQUIRE_BUNDLE("Require-Bundle");
const std::string BundleManifest::REQUIRE_MODULE("Require-Module");
const std::string BundleManifest::PROVIDE_MODULE("Provide-Module");
const std::string BundleManifest::EXTENDS_BUNDLE("Extends-Bundle");
const std::string BundleManifest::BUNDLE_LAZYSTART("Bundle-LazyStart");
const std::string BundleManifest::BUNDLE_SEALED("Bundle-Sealed");
const std::string BundleManifest::BUNDLE_PREVENTUNINSTALL("Bundle-PreventUninstall");
const std::string BundleManifest::BUNDLE_RUNLEVEL("Bundle-RunLevel");
const std::string BundleManifest::BUNDLE_CERTIFICATE("Bundle-Certificate");
const std::string BundleManifest::DEFAULT_RUNLEVEL("999-user");


BundleManifest::BundleManifest(std::istream& istr):
	_lazyStart(false),
	_sealed(false),
	_preventUninstall(false),
	_pManifest(new Poco::Util::PropertyFileConfiguration(istr))
{
	parseManifest();
}


BundleManifest::~BundleManifest()
{
}


void BundleManifest::parseManifest()
{
	for (const std::string& header: {MANIFEST_VERSION, BUNDLE_NAME, BUNDLE_SYMBOLICNAME, BUNDLE_VERSION})
	{
		if (!_pManifest->hasProperty(header)) throw ManifestException(header, "The header is missing");
	}

	std::string manifestVersion = _pManifest->getString(MANIFEST_VERSION);
	if (manifestVersion != VERSION)
		throw ManifestException("Unsupported manifest version", manifestVersion);

	_name             = _pManifest->getString(BUNDLE_NAME);
	_symbolicName     = _pManifest->getString(BUNDLE_SYMBOLICNAME);
	_version          = parseVersion(BUNDLE_VERSION, _pManifest->getString(BUNDLE_VERSION));
	_vendor           = _pManifest->getString(BUNDLE_VENDOR, "");
	_copyright        = _pManifest->getString(BUNDLE_COPYRIGHT, "");
	_lazyStart        = _pManifest->getBool(BUNDLE_LAZYSTART, false);
	_sealed           = _pManifest->getBool(BUNDLE_SEALED, false);
	_preventUninstall = _pManifest->getBool(BUNDLE_PREVENTUNINSTALL, false);
	_runLevel         = _pManifest->getString(BUNDLE_RUNLEVEL, DEFAULT_RUNLEVEL);
	_extendedBundle   = _pManifest->getString(EXTENDS_BUNDLE, "");

	if (!isValidFileName(_symbolicName))
		throw ManifestException("Bundle symbolic name cannot be used as a file name", _symbolicName);
	if (!_version.designation().empty() && !isValidFileName(_version.designation()))
		throw ManifestException("Bundle version designation cannot be used as a file name", _version.toString());

	parseActivator(_pManifest->getString(BUNDLE_ACTIVATOR, ""));
	parseRequiredBundles(_pManifest->getString(REQUIRE_BUNDLE, ""));
	parseRequiredModules(_pManifest->getString(REQUIRE_MODULE, ""));
	parseProvidedModules(_pManifest->getString(PROVIDE_MODULE, ""));
}


bool BundleManifest::isValidFileName(const std::string& name)
{
	if (name.empty() || name[0] == '.') return false;

	return std::none_of(name.begin(), name.end(), [](char c)
	{
		return c == '/' || c == '\\' || c == ':'
			|| Poco::Ascii::hasProperties(static_cast<unsigned char>(c), Poco::Ascii::ACP_CONTROL);
	});
}


void BundleManifest::parseActivator(const std::string& activator)
{
	std::string clazz;
	std::string library;

	std::string::const_iterator it(activator.begin());
	std::string::const_iterator end(activator.end());

	while (it != end && Poco::Ascii::isSpace(*it)) ++it;
	while (it != end && !Poco::Ascii::isSpace(*it) && *it != ';') clazz += *it++;
	while (it != end && Poco::Ascii::isSpace(*it)) ++it;
	if (it != end)
	{
		if (*it == ';')
		{
			++it;
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
			std::string keyword;
			while (it != end && !Poco::Ascii::isSpace(*it) && *it != '=') keyword += *it++;
			if (keyword != "library") throw ManifestException(BUNDLE_ACTIVATOR, "Unknown keyword following activator class name");
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
			if (it == end || *it != '=') throw ManifestException(BUNDLE_ACTIVATOR, "The library keyword must be followed by '='");
			++it;
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
			while (it != end && !Poco::Ascii::isSpace(*it)) library += *it++;
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
		}
		if (it != end) throw ManifestException(BUNDLE_ACTIVATOR, "Invalid characters after activator class name");
		if (!library.empty() && !isValidFileName(library)) throw ManifestException(BUNDLE_ACTIVATOR, "The library name cannot be used as a file name");
	}
	std::swap(clazz, _activatorClass);
	std::swap(library, _activatorLibrary);
}


void BundleManifest::parseRequiredBundles(const std::string& requiredBundles)
{
	parseRequiredItems(requiredBundles, REQUIRE_BUNDLE, "bundle-version", _requiredBundles);
}


void BundleManifest::parseRequiredModules(const std::string& requiredModules)
{
	parseRequiredItems(requiredModules, REQUIRE_MODULE, "module-version", _requiredModules);
}


void BundleManifest::parseRequiredItems(const std::string& requiredItems, const std::string& what, const std::string& versionKeyword, Dependencies& dependencies)
{
	std::string::const_iterator it(requiredItems.begin());
	std::string::const_iterator end(requiredItems.end());

	while (it != end)
	{
		while (it != end && Poco::Ascii::isSpace(*it)) ++it;
		std::string name;
		while (it != end && !Poco::Ascii::isSpace(*it) && *it != ',' && *it != ';') name += *it++;
		if (name.empty() && it != end && *it != ',') throw ManifestException(what, "Expected an item name");
		if (!name.empty())
		{
			Dependency dependency;
			dependency.symbolicName = name;
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
			if (it != end && *it == ';')
			{
				++it;
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				std::string keyword;
				while (it != end && !Poco::Ascii::isSpace(*it) && *it != '=') keyword += *it++;
				if (keyword != versionKeyword) throw ManifestException(what, "Unknown keyword following item name");
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				if (it == end || *it != '=') throw ManifestException(what, Poco::format("The %s keyword must be followed by '='"s, versionKeyword));
				++it;
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				if (it != end)
				{
					if (*it == '(' || *it == '[')
					{
						bool includeLower = *it == '[';
						++it;
						while (it != end && Poco::Ascii::isSpace(*it)) ++it;
						std::string lowerStr;
						while (it != end && !Poco::Ascii::isSpace(*it) && *it != ',') lowerStr += *it++;
						while (it != end && Poco::Ascii::isSpace(*it)) ++it;
						if (it != end && *it == ',')
						{
							++it;
							while (it != end && Poco::Ascii::isSpace(*it)) ++it;
							std::string upperStr;
							while (it != end && !Poco::Ascii::isSpace(*it) && *it != ')' && *it != ']') upperStr += *it++;
							while (it != end && Poco::Ascii::isSpace(*it)) ++it;
							if (it != end && (*it == ')' || *it == ']'))
							{
								bool includeUpper = *it == ']';
								++it;
								Version lower(parseVersion(what, lowerStr));
								Version upper(parseVersion(what, upperStr));
								if (upper < lower) throw ManifestException(what, "The lower bound of a version range is above its upper bound");
								if (upper == lower && !(includeLower && includeUpper)) throw ManifestException(what, "The version range contains no version");
								dependency.versions = VersionRange(lower, includeLower, upper, includeUpper);
							}
							else throw ManifestException(what, "Invalid version range specified");
						}
						else throw ManifestException(what, "Invalid version range specified");
					}
					else
					{
						std::string versionStr;
						while (it != end && !Poco::Ascii::isSpace(*it) && *it != ',') versionStr += *it++;
						Version version(parseVersion(what, versionStr));
						dependency.versions = VersionRange(version, true, version, true);
					}
				}
				else throw ManifestException(what, Poco::format("The %s keyword must be followed by a version or version range"s, versionKeyword));
			}
			skipItemEnd(what, it, end);
			dependencies.push_back(dependency);
		}
		if (it != end && *it == ',') ++it;
	}
}


void BundleManifest::parseProvidedModules(const std::string& providedModules)
{
	std::string::const_iterator it(providedModules.begin());
	std::string::const_iterator end(providedModules.end());

	while (it != end)
	{
		while (it != end && Poco::Ascii::isSpace(*it)) ++it;
		std::string name;
		while (it != end && !Poco::Ascii::isSpace(*it) && *it != ',' && *it != ';') name += *it++;
		if (name.empty() && it != end && *it != ',') throw ManifestException(PROVIDE_MODULE, "Expected an item name");
		if (!name.empty())
		{
			Module module;
			module.symbolicName = name;
			while (it != end && Poco::Ascii::isSpace(*it)) ++it;
			if (it != end && *it == ';')
			{
				++it;
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				std::string keyword;
				while (it != end && !Poco::Ascii::isSpace(*it) && *it != '=') keyword += *it++;
				if (keyword != "module-version") throw ManifestException(PROVIDE_MODULE, "Unknown keyword following module name");
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				if (it == end || *it != '=') throw ManifestException(PROVIDE_MODULE, "The module-version keyword must be followed by '='");
				++it;
				while (it != end && Poco::Ascii::isSpace(*it)) ++it;
				if (it != end)
				{
					std::string versionStr;
					while (it != end && !Poco::Ascii::isSpace(*it) && *it != ',') versionStr += *it++;
					module.version = parseVersion(PROVIDE_MODULE, versionStr);
				}
				else throw ManifestException(PROVIDE_MODULE, "The module-version keyword must be followed by a version");
			}
			else throw ManifestException(PROVIDE_MODULE, "No module-version specified");
			skipItemEnd(PROVIDE_MODULE, it, end);
			_providedModules.push_back(module);
		}
		if (it != end && *it == ',') ++it;
	}
}


} } // namespace Poco::OSP
