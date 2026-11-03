//
// BundleManifestTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleManifestTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/Version.h"
#include "Poco/Event.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


#undef major
#undef minor


using Poco::OSP::BundleManifest;
using Poco::OSP::Version;


namespace
{
	constexpr long PARSE_WAIT_MS = 10000;


	[[nodiscard]] bool isRejectedInTime(const std::string& manifest)
		/// Parses the manifest in a thread of its own and returns true if
		/// the parser throws a ManifestException within PARSE_WAIT_MS.
		/// A parser that does not return is left running in its detached
		/// thread, so that the test fails instead of blocking the runner.
	{
		auto pDone = std::make_shared<Poco::Event>();
		auto pRejected = std::make_shared<std::atomic<bool>>(false);
		std::thread parser([manifest, pDone, pRejected]()
		{
			std::istringstream istr(manifest);
			try
			{
				BundleManifest::Ptr pManifest(new BundleManifest(istr));
			}
			catch (Poco::OSP::ManifestException&)
			{
				*pRejected = true;
			}
			catch (...)
			{
			}
			pDone->set();
		});
		const bool returned = pDone->tryWait(PARSE_WAIT_MS);
		if (returned)
			parser.join();
		else
			parser.detach();
		return returned && *pRejected;
	}
}


BundleManifestTest::BundleManifestTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleManifestTest::~BundleManifestTest() = default;


void BundleManifestTest::testBasicPropertiesAreRead()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-Vendor: Applied Informatics\n"
		"Bundle-Copyright: (c) 2007, Applied Informatics\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->name() == "Sample Bundle");
	assert (pManifest->vendor() == "Applied Informatics");
	assert (pManifest->copyright() == "(c) 2007, Applied Informatics");
	assert (pManifest->symbolicName() == "com.appinf.osp.sample");
	assert (pManifest->version().major() == 1);
	assert (pManifest->version().minor() == 0);
	assert (pManifest->version().revision() == 2);
	assert (pManifest->version().designation() == "b1");
	assert (!pManifest->lazyStart());
}


void BundleManifestTest::testMissingPropertiesAreEmptyAndLazyStartIsRead()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: ${bundleName}\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.2.3\n"
		"Bundle-LazyStart: true\n"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->name() == "${bundleName}");
	assert (pManifest->vendor() == "");
	assert (pManifest->copyright() == "");
	assert (pManifest->symbolicName() == "com.appinf.osp.sample");
	assert (pManifest->version().major() == 1);
	assert (pManifest->version().minor() == 2);
	assert (pManifest->version().revision() == 3);
	assert (pManifest->version().designation() == "");
	assert (pManifest->activatorClass() == "");
	assert (pManifest->activatorLibrary() == "");
	assert (pManifest->requiredBundles().empty());
	assert (pManifest->lazyStart());
}


void BundleManifestTest::testActivatorWithoutLibrary()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Bundle-Activator: SampleActivator\n"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->activatorClass() == "SampleActivator");
	assert (pManifest->activatorLibrary() == "");
}


void BundleManifestTest::testActivatorWithLibrary()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Bundle-Activator: SampleActivator;library=SampleLib\n"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->activatorClass() == "SampleActivator");
	assert (pManifest->activatorLibrary() == "SampleLib");
}


void BundleManifestTest::testActivatorNamespacedClassAndSpaces()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Bundle-Activator: NS::SampleActivator ; library = SampleLib \n"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->activatorClass() == "NS::SampleActivator");
	assert (pManifest->activatorLibrary() == "SampleLib");
}


void BundleManifestTest::testRequiredBundleWithoutVersionMatchesAny()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 1);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.isEmpty());
}


void BundleManifestTest::testRequiredBundleSingleVersion()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=1.1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 1);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound() == pManifest->requiredBundles()[0].versions.lowerBound());
	assert (pManifest->requiredBundles()[0].versions.includeLower());
	assert (pManifest->requiredBundles()[0].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleSingleVersionWithSpaces()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1 ; bundle-version = 1.1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 1);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound() == pManifest->requiredBundles()[0].versions.lowerBound());
	assert (pManifest->requiredBundles()[0].versions.includeLower());
	assert (pManifest->requiredBundles()[0].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleVersionAppliesToItsBundle()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1, com.appinf.osp.bundle2; bundle-version=1.2.3"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 2);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.isEmpty());
	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 3);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleVersionBetweenBundlesWithoutVersion()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1, com.appinf.osp.bundle2; bundle-version=1.2.3, com.appinf.osp.bundle3"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 3);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.isEmpty());
	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 3);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
	assert (pManifest->requiredBundles()[2].symbolicName == "com.appinf.osp.bundle3");
	assert (pManifest->requiredBundles()[2].versions.isEmpty());
}


void BundleManifestTest::testRequiredBundleRangeIncludingLowerBound()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=[1.1,1.2), com.appinf.osp.bundle2;bundle-version=1.2"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 2);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.upperBound().minor() == 2);
	assert (pManifest->requiredBundles()[0].versions.upperBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.includeLower());
	assert (!pManifest->requiredBundles()[0].versions.includeUpper());

	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleRangeIncludingUpperBound()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=(1.1,1.2], com.appinf.osp.bundle2;bundle-version=1.2"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 2);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.upperBound().minor() == 2);
	assert (pManifest->requiredBundles()[0].versions.upperBound().revision() == 0);
	assert (!pManifest->requiredBundles()[0].versions.includeLower());
	assert (pManifest->requiredBundles()[0].versions.includeUpper());

	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleOpenRange()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=(1.1,1.2), com.appinf.osp.bundle2;bundle-version=1.2"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 2);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.upperBound().minor() == 2);
	assert (pManifest->requiredBundles()[0].versions.upperBound().revision() == 0);
	assert (!pManifest->requiredBundles()[0].versions.includeLower());
	assert (!pManifest->requiredBundles()[0].versions.includeUpper());

	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
}


void BundleManifestTest::testRequiredBundleWhitespaceIgnored()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Bundle: com.appinf.osp.bundle1 ; bundle-version = ( 1.1 , 1.2 ) , com.appinf.osp.bundle2   ;bundle-version = 1.2"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredBundles().size() == 2);
	assert (pManifest->requiredBundles()[0].symbolicName == "com.appinf.osp.bundle1");
	assert (pManifest->requiredBundles()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredBundles()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[0].versions.upperBound().major() == 1);
	assert (pManifest->requiredBundles()[0].versions.upperBound().minor() == 2);
	assert (pManifest->requiredBundles()[0].versions.upperBound().revision() == 0);
	assert (!pManifest->requiredBundles()[0].versions.includeLower());
	assert (!pManifest->requiredBundles()[0].versions.includeUpper());

	assert (pManifest->requiredBundles()[1].symbolicName == "com.appinf.osp.bundle2");
	assert (pManifest->requiredBundles()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredBundles()[1].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredBundles()[1].versions.upperBound() == pManifest->requiredBundles()[1].versions.lowerBound());
	assert (pManifest->requiredBundles()[1].versions.includeLower());
	assert (pManifest->requiredBundles()[1].versions.includeUpper());
}


void BundleManifestTest::testRequiredModuleWithoutVersionMatchesAny()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Module: com.appinf.osp.module1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredModules().size() == 1);
	assert (pManifest->requiredModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->requiredModules()[0].versions.isEmpty());
}


void BundleManifestTest::testRequiredModuleSingleVersion()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Module: com.appinf.osp.module1;module-version=1.1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredModules().size() == 1);
	assert (pManifest->requiredModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->requiredModules()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredModules()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredModules()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredModules()[0].versions.upperBound() == pManifest->requiredModules()[0].versions.lowerBound());
	assert (pManifest->requiredModules()[0].versions.includeLower());
	assert (pManifest->requiredModules()[0].versions.includeUpper());
}


void BundleManifestTest::testRequiredModuleSingleVersionWithSpaces()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Module: com.appinf.osp.module1 ; module-version = 1.1"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredModules().size() == 1);
	assert (pManifest->requiredModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->requiredModules()[0].versions.lowerBound().major() == 1);
	assert (pManifest->requiredModules()[0].versions.lowerBound().minor() == 1);
	assert (pManifest->requiredModules()[0].versions.lowerBound().revision() == 0);
	assert (pManifest->requiredModules()[0].versions.upperBound() == pManifest->requiredModules()[0].versions.lowerBound());
	assert (pManifest->requiredModules()[0].versions.includeLower());
	assert (pManifest->requiredModules()[0].versions.includeUpper());
}


void BundleManifestTest::testRequiredModuleVersionAppliesToItsModule()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Require-Module: com.appinf.osp.module1, com.appinf.osp.module2; module-version=1.2.3"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->requiredModules().size() == 2);
	assert (pManifest->requiredModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->requiredModules()[0].versions.isEmpty());
	assert (pManifest->requiredModules()[1].symbolicName == "com.appinf.osp.module2");
	assert (pManifest->requiredModules()[1].versions.lowerBound().major() == 1);
	assert (pManifest->requiredModules()[1].versions.lowerBound().minor() == 2);
	assert (pManifest->requiredModules()[1].versions.lowerBound().revision() == 3);
	assert (pManifest->requiredModules()[1].versions.upperBound() == pManifest->requiredModules()[1].versions.lowerBound());
	assert (pManifest->requiredModules()[1].versions.includeLower());
	assert (pManifest->requiredModules()[1].versions.includeUpper());
}


void BundleManifestTest::testProvidedModuleWithVersion()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Provide-Module: com.appinf.osp.module1; module-version=1.0"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->providedModules().size() == 1);
	assert (pManifest->providedModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->providedModules()[0].version.major() == 1);
	assert (pManifest->providedModules()[0].version.minor() == 0);
	assert (pManifest->providedModules()[0].version.revision() == 0);
}


void BundleManifestTest::testProvidedModules()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Provide-Module: com.appinf.osp.module1;module-version=1.0,com.appinf.osp.module2;module-version=2.1.3"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->providedModules().size() == 2);
	assert (pManifest->providedModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->providedModules()[0].version.major() == 1);
	assert (pManifest->providedModules()[0].version.minor() == 0);
	assert (pManifest->providedModules()[0].version.revision() == 0);
	assert (pManifest->providedModules()[1].symbolicName == "com.appinf.osp.module2");
	assert (pManifest->providedModules()[1].version.major() == 2);
	assert (pManifest->providedModules()[1].version.minor() == 1);
	assert (pManifest->providedModules()[1].version.revision() == 3);
}


void BundleManifestTest::testProvidedModulesWhitespaceIgnored()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Provide-Module: com.appinf.osp.module1 ; module-version = 1.0, com.appinf.osp.module2; module-version = 2.1.3"
	);
	std::istringstream istr(manifestData);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assert (pManifest->providedModules().size() == 2);
	assert (pManifest->providedModules()[0].symbolicName == "com.appinf.osp.module1");
	assert (pManifest->providedModules()[0].version.major() == 1);
	assert (pManifest->providedModules()[0].version.minor() == 0);
	assert (pManifest->providedModules()[0].version.revision() == 0);
	assert (pManifest->providedModules()[1].symbolicName == "com.appinf.osp.module2");
	assert (pManifest->providedModules()[1].version.major() == 2);
	assert (pManifest->providedModules()[1].version.minor() == 1);
	assert (pManifest->providedModules()[1].version.revision() == 3);
}


void BundleManifestTest::testProvidedModuleWithoutVersionIsRejected()
{
	std::string manifestData(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.2-b1\n"
		"Provide-Module: com.appinf.osp.module1"
	);
	try
	{
		std::istringstream istr(manifestData);
		BundleManifest::Ptr pManifest(new BundleManifest(istr));
		fail("no module version specified - must throw");
	}
	catch (Poco::OSP::ManifestException&)
	{
	}
}


void BundleManifestTest::testSymbolicNameThatIsNoFileNameIsRejected()
{
	// written as they appear in the manifest file, which is a property file
	const std::vector<std::string> names = {"../victim", "a/b", "a\\\\b", "c:x", ".hidden", "..", "a\\tb", ""};
	for (const auto& name: names)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: " + name + "\n"
			"Bundle-Version: 1.0.0\n"
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("symbolic name '" + name + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testSymbolicNameWithUnderscoreIsAccepted()
{
	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.example_bundle-1\n"
		"Bundle-Version: 1.0.0\n"
	);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assertTrue (pManifest->symbolicName() == "com.example_bundle-1");
}


void BundleManifestTest::testVersionDesignationThatIsNoFileNameIsRejected()
{
	const std::vector<std::string> versions = {"1.0.0-a/b", "1.0.0-/../../x", "1.0.0-..", "1.0.0-c:x"};
	for (const auto& version: versions)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n"
			"Bundle-Version: " + version + "\n"
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("version '" + version + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testActivatorLibraryThatIsNoFileNameIsRejected()
{
	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
		"Bundle-Activator: SampleActivator;library=../SampleLib\n"
	);
	try
	{
		BundleManifest::Ptr pManifest(new BundleManifest(istr));
		fail("library name with a path must be rejected");
	}
	catch (Poco::OSP::ManifestException&)
	{
	}
}


void BundleManifestTest::testReversedVersionRangeIsRejected()
{
	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
		"Require-Bundle: com.appinf.osp.other;bundle-version=[2.0,1.0]\n"
	);
	try
	{
		BundleManifest::Ptr pManifest(new BundleManifest(istr));
		fail("reversed version range must be rejected");
	}
	catch (Poco::OSP::ManifestException&)
	{
	}
}


void BundleManifestTest::testEmptyBundleVersionIsRejected()
{
	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version:\n"
	);
	try
	{
		BundleManifest::Ptr pManifest(new BundleManifest(istr));
		fail("empty version must be rejected");
	}
	catch (Poco::OSP::ManifestException&)
	{
	}
}


void BundleManifestTest::testMissingRequiredHeaderIsRejected()
{
	const std::vector<std::string> headers = {
		"Manifest-Version: 1.0\n",
		"Bundle-Name: Sample Bundle\n",
		"Bundle-SymbolicName: com.appinf.osp.sample\n",
		"Bundle-Version: 1.0.0\n"
	};
	for (std::size_t missing = 0; missing < headers.size(); ++missing)
	{
		std::string manifestData;
		for (std::size_t i = 0; i < headers.size(); ++i)
		{
			if (i != missing) manifestData += headers[i];
		}
		std::istringstream istr(manifestData);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("manifest without '" + headers[missing] + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testMalformedVersionIsRejected()
{
	const std::vector<std::string> headers = {
		"Bundle-Version: 1.x\n",
		"Bundle-Version: 1.0.0\nRequire-Bundle: com.appinf.osp.bundle1;bundle-version=1.x\n",
		"Bundle-Version: 1.0.0\nRequire-Bundle: com.appinf.osp.bundle1;bundle-version=[1.0,x)\n",
		"Bundle-Version: 1.0.0\nRequire-Module: com.appinf.osp.module1;module-version=1..0\n",
		"Bundle-Version: 1.0.0\nProvide-Module: com.appinf.osp.module1;module-version=x\n"
	};
	for (const auto& header: headers)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n" + header
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("manifest with '" + header + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testVersionRangeThatIsNotClosedIsRejected()
{
	const std::vector<std::string> ranges = {"[1.0", "(1.0", "[1.0,2.0", "[1.0 2.0]"};
	for (const auto& range: ranges)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n"
			"Bundle-Version: 1.0.0\n"
			"Require-Bundle: com.appinf.osp.bundle1;bundle-version=" + range + "\n"
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("version range '" + range + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testVersionRangeWithoutVersionIsRejected()
{
	const std::vector<std::string> ranges = {"(1.0.0,1.0.0]", "[1.0.0,1.0.0)", "(1.0.0,1.0.0)"};
	for (const auto& range: ranges)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n"
			"Bundle-Version: 1.0.0\n"
			"Require-Module: com.appinf.osp.module1;module-version=" + range + "\n"
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("version range '" + range + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}

	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
		"Require-Module: com.appinf.osp.module1;module-version=[1.0.0,1.0.0]\n"
	);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assertTrue (pManifest->requiredModules()[0].versions.isInRange(Version(1, 0, 0)));
}


void BundleManifestTest::testItemsNotSeparatedByCommaAreRejected()
{
	const std::vector<std::string> headers = {
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=1.0.0 com.appinf.osp.bundle2",
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=[1.0.0,2.0.0]com.appinf.osp.bundle2",
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=(1.0.0,2.0.0) com.appinf.osp.bundle2",
		"Require-Bundle: com.appinf.osp.bundle1 com.appinf.osp.bundle2",
		"Provide-Module: com.appinf.osp.module1;module-version=1.0.0 com.appinf.osp.module2;module-version=2.0.0"
	};
	for (const auto& header: headers)
	{
		std::istringstream istr(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n"
			"Bundle-Version: 1.0.0\n" +
			header + "\n"
		);
		try
		{
			BundleManifest::Ptr pManifest(new BundleManifest(istr));
			failmsg("header '" + header + "' must be rejected");
		}
		catch (Poco::OSP::ManifestException&)
		{
		}
	}
}


void BundleManifestTest::testItemWithoutNameIsRejected()
{
	const std::vector<std::string> headers = {
		"Require-Bundle: ;bundle-version=1.0.0",
		"Require-Bundle: com.appinf.osp.bundle1, ;com.appinf.osp.bundle2",
		"Require-Module: ;module-version=1.0.0",
		"Provide-Module: ;module-version=1.0.0"
	};
	for (const auto& header: headers)
	{
		const std::string manifest(
			"Manifest-Version: 1.0\n"
			"Bundle-Name: Sample Bundle\n"
			"Bundle-SymbolicName: com.appinf.osp.sample\n"
			"Bundle-Version: 1.0.0\n" +
			header + "\n"
		);
		if (!isRejectedInTime(manifest)) failmsg("header '" + header + "' must be rejected");
	}
}


void BundleManifestTest::testFlagsRunLevelAndExtendedBundle()
{
	std::istringstream defaults(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
	);
	BundleManifest::Ptr pDefaults(new BundleManifest(defaults));
	assertFalse (pDefaults->sealed());
	assertFalse (pDefaults->preventUninstall());
	assertEqual (BundleManifest::DEFAULT_RUNLEVEL, pDefaults->runLevel());
	assertEqual ("999-user", pDefaults->runLevel());
	assertEqual ("", pDefaults->extendedBundle());

	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
		"Bundle-Sealed: true\n"
		"Bundle-PreventUninstall: true\n"
		"Bundle-RunLevel: 100-system\n"
		"Extends-Bundle: com.appinf.osp.base\n"
	);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	assertTrue (pManifest->sealed());
	assertTrue (pManifest->preventUninstall());
	assertEqual ("100-system", pManifest->runLevel());
	assertEqual ("com.appinf.osp.base", pManifest->extendedBundle());
}


void BundleManifestTest::testUnknownHeaderIsIgnoredAndLinesContinue()
{
	std::istringstream istr(
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Sample Bundle\n"
		"Bundle-SymbolicName: com.appinf.osp.sample\n"
		"Bundle-Version: 1.0.0\n"
		"X-Unknown-Header: anything\n"
		"Require-Bundle: com.appinf.osp.bundle1;bundle-version=[1.0,2.0), \\\n"
		"                com.appinf.osp.bundle2\n"
	);
	BundleManifest::Ptr pManifest(new BundleManifest(istr));
	const BundleManifest::Dependencies& deps = pManifest->requiredBundles();
	assertEqual (2, static_cast<int>(deps.size()));
	assertEqual ("com.appinf.osp.bundle1", deps[0].symbolicName);
	assertEqual ("[1.0.0,2.0.0)", deps[0].versions.toString());
	assertEqual ("com.appinf.osp.bundle2", deps[1].symbolicName);
	assertTrue (deps[1].versions.isEmpty());
}


void BundleManifestTest::setUp()
{
}


void BundleManifestTest::tearDown()
{
}


CppUnit::Test* BundleManifestTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleManifestTest");

	CppUnit_addTest(pSuite, BundleManifestTest, testBasicPropertiesAreRead);
	CppUnit_addTest(pSuite, BundleManifestTest, testMissingPropertiesAreEmptyAndLazyStartIsRead);
	CppUnit_addTest(pSuite, BundleManifestTest, testActivatorWithoutLibrary);
	CppUnit_addTest(pSuite, BundleManifestTest, testActivatorWithLibrary);
	CppUnit_addTest(pSuite, BundleManifestTest, testActivatorNamespacedClassAndSpaces);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleWithoutVersionMatchesAny);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleSingleVersion);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleSingleVersionWithSpaces);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleVersionAppliesToItsBundle);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleVersionBetweenBundlesWithoutVersion);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleRangeIncludingLowerBound);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleRangeIncludingUpperBound);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleOpenRange);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredBundleWhitespaceIgnored);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredModuleWithoutVersionMatchesAny);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredModuleSingleVersion);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredModuleSingleVersionWithSpaces);
	CppUnit_addTest(pSuite, BundleManifestTest, testRequiredModuleVersionAppliesToItsModule);
	CppUnit_addTest(pSuite, BundleManifestTest, testProvidedModuleWithVersion);
	CppUnit_addTest(pSuite, BundleManifestTest, testProvidedModules);
	CppUnit_addTest(pSuite, BundleManifestTest, testProvidedModulesWhitespaceIgnored);
	CppUnit_addTest(pSuite, BundleManifestTest, testProvidedModuleWithoutVersionIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testSymbolicNameThatIsNoFileNameIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testSymbolicNameWithUnderscoreIsAccepted);
	CppUnit_addTest(pSuite, BundleManifestTest, testVersionDesignationThatIsNoFileNameIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testActivatorLibraryThatIsNoFileNameIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testReversedVersionRangeIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testEmptyBundleVersionIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testMissingRequiredHeaderIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testMalformedVersionIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testVersionRangeThatIsNotClosedIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testVersionRangeWithoutVersionIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testItemsNotSeparatedByCommaAreRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testItemWithoutNameIsRejected);
	CppUnit_addTest(pSuite, BundleManifestTest, testFlagsRunLevelAndExtendedBundle);
	CppUnit_addTest(pSuite, BundleManifestTest, testUnknownHeaderIsIgnoredAndLinesContinue);

	return pSuite;
}
