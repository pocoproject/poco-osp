//
// TestLoader.h
//
// Definition of the TestLoader class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestLoader_INCLUDED
#define TestLoader_INCLUDED


#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include <string>


class TestLoader
	/// A BundleLoader for the tests, together with the code cache,
	/// service registry and system events it works with.
{
public:
	TestLoader() = delete;
	explicit TestLoader(const std::string& codeCachePath,
		const Poco::OSP::LanguageTag& language = Poco::OSP::LanguageTag("en", "US"));
		/// Creates the BundleLoader with its code cache in the given
		/// directory and bundles in the given language.
	TestLoader(const TestLoader&) = delete;
	TestLoader(TestLoader&&) = delete;
	~TestLoader();

	TestLoader& operator = (const TestLoader&) = delete;
	TestLoader& operator = (TestLoader&&) = delete;

	[[nodiscard]] Poco::OSP::BundleLoader& loader();
		/// Returns the bundle loader.

	[[nodiscard]] Poco::OSP::CodeCache& codeCache();
		/// Returns the code cache of the bundle loader.

	[[nodiscard]] Poco::OSP::ServiceRegistry& registry();
		/// Returns the service registry of the bundle contexts.

	[[nodiscard]] Poco::OSP::SystemEvents& systemEvents();
		/// Returns the system events of the bundle contexts.

private:
	Poco::OSP::CodeCache _codeCache;
	Poco::OSP::ServiceRegistry _registry;
	Poco::OSP::SystemEvents _systemEvents;
	Poco::OSP::BundleLoader _loader;
};


#endif // TestLoader_INCLUDED
