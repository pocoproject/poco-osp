//
// ManifestInfo.h
//
// Declaration of the ManifestInfo class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleCreator_ManifestInfo_INCLUDED
#define BundleCreator_ManifestInfo_INCLUDED


#include "Poco/OSP/Version.h"
#include "Poco/OSP/VersionRange.h"
#include <string>
#include <vector>


class ManifestInfo
	/// The manifest element of a bundle specification file, from which the Bundle Creator writes META-INF/manifest.mf.
{
public:
	struct Dependency
	{
		std::string symbolicName;
		std::string versions;
	};
	using Dependencies = std::vector<Dependency>;

	struct ProvidedModule
	{
		std::string symbolicName;
		std::string version;
	};
	using ProvidedModules = std::vector<ProvidedModule>;

	ManifestInfo(const std::string& name,
		const std::string& symbolicName,
		const Poco::OSP::Version& version,
		const std::string& vendor,
		const std::string& copyright,
		const std::string& activatorClass,
		const std::string& activatorLibrary,
		const Dependencies& requiredBundles,
		const Dependencies& requiredModules,
		const ProvidedModules& providedModules,
		bool lazyStart,
		bool sealed,
		bool preventUninstall,
		const std::string& runLevel,
		const std::string& extendsBundle);

	~ManifestInfo();

	[[nodiscard]] const std::string& name() const;
	[[nodiscard]] const std::string& symbolicName() const;
	[[nodiscard]] Poco::OSP::Version version() const;
	[[nodiscard]] const std::string& vendor() const;
	[[nodiscard]] const std::string& copyright() const;
	[[nodiscard]] const std::string& activatorClass() const;
	[[nodiscard]] const std::string& activatorLibrary() const;
	[[nodiscard]] const Dependencies& requiredBundles() const;
	[[nodiscard]] const Dependencies& requiredModules() const;
	[[nodiscard]] const ProvidedModules& providedModules() const;
	[[nodiscard]] bool lazyStart() const;
	[[nodiscard]] bool sealed() const;
	[[nodiscard]] bool preventUninstall() const;
	[[nodiscard]] const std::string& runLevel() const;
	[[nodiscard]] const std::string& extendsBundle() const;

private:
	std::string  _name;
	std::string  _symbolicName;
	Poco::OSP::Version _version;
	std::string  _vendor;
	std::string  _copyright;
	std::string  _activatorClass;
	std::string  _activatorLibrary;
	Dependencies _requiredBundles;
	Dependencies _requiredModules;
	ProvidedModules _providedModules;
	bool         _lazyStart;
	bool         _sealed;
	bool         _preventUninstall;
	std::string  _runLevel;
	std::string  _extendsBundle;
};


//
// inlines
//
inline const std::string& ManifestInfo::name() const
{
	return _name;
}


inline const std::string& ManifestInfo::symbolicName() const
{
	return _symbolicName;
}


inline Poco::OSP::Version ManifestInfo::version() const
{
	return _version;
}


inline const std::string& ManifestInfo::vendor() const
{
	return _vendor;
}


inline const std::string& ManifestInfo::copyright() const
{
	return _copyright;
}


inline const std::string& ManifestInfo::activatorClass() const
{
	return _activatorClass;
}


inline const std::string& ManifestInfo::activatorLibrary() const
{
	return _activatorLibrary;
}


inline const ManifestInfo::Dependencies& ManifestInfo::requiredBundles() const
{
	return _requiredBundles;
}


inline const ManifestInfo::Dependencies& ManifestInfo::requiredModules() const
{
	return _requiredModules;
}


inline const ManifestInfo::ProvidedModules& ManifestInfo::providedModules() const
{
	return _providedModules;
}


inline bool ManifestInfo::lazyStart() const
{
	return _lazyStart;
}


inline bool ManifestInfo::sealed() const
{
	return _sealed;
}


inline bool ManifestInfo::preventUninstall() const
{
	return _preventUninstall;
}


inline const std::string& ManifestInfo::runLevel() const
{
	return _runLevel;
}


inline const std::string& ManifestInfo::extendsBundle() const
{
	return _extendsBundle;
}


#endif // BundleCreator_ManifestInfo_INCLUDED
