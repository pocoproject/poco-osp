//
// OSPException.cpp
//
// Library: OSP
// Package: Core
// Module:  OSPException
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/OSPException.h"


namespace Poco::OSP {


POCO_IMPLEMENT_EXCEPTION(OSPException, Poco::RuntimeException, "OSP Exception")
POCO_IMPLEMENT_EXCEPTION(BundleException, OSPException, "Bundle error")
POCO_IMPLEMENT_EXCEPTION(ManifestException, BundleException, "Manifest error")
POCO_IMPLEMENT_EXCEPTION(BundleStateException, BundleException, "Invalid bundle state")
POCO_IMPLEMENT_EXCEPTION(BundleVersionConflictException, BundleException, "Bundle version conflict")
POCO_IMPLEMENT_EXCEPTION(BundleLoadException, BundleException, "Cannot load bundle")
POCO_IMPLEMENT_EXCEPTION(BundleResolveException, BundleException, "Cannot resolve bundle")
POCO_IMPLEMENT_EXCEPTION(BundleInstallException, BundleException, "Cannot install bundle")
POCO_IMPLEMENT_EXCEPTION(BundleUninstallException, BundleException, "Cannot uninstall bundle")
POCO_IMPLEMENT_EXCEPTION(BundleSealedException, BundleException, "Bundle is sealed")


} // namespace Poco::OSP
