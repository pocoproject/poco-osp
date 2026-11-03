//
// BundleInstallerService.h
//
// Library: OSP
// Package: BundleInstallerService
// Module:  BundleInstallerService
//
// Definition of the BundleInstallerService class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_BundleInstallerService_INCLUDED
#define OSP_BundleInstallerService_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/Bundle.h"
#include <istream>


namespace Poco::OSP {


class BundleRepository;


class OSP_API BundleInstallerService: public Service
	/// The BundleInstallerService allows a bundle to install
	/// other bundles, either from a stream or from a
	/// URI.
	///
	/// The service name of the BundleInstallerService
	/// is "osp.core.installer".
{
public:
	using Ptr = Poco::AutoPtr<BundleInstallerService>;
	using ConstPtr = const Ptr;

	BundleInstallerService(BundleRepository& repository);
		/// Creates the BundleInstallerService, using
		/// the given BundleRepository.

	BundleInstallerService() = delete;

	Bundle::Ptr installBundle(const std::string& uri);
		/// Installs a bundle from the given URI.
		/// The Poco::URIStreamOpener class is used to
		/// open a stream for the given URI.
		///
		/// Any URI that the default URIStreamOpener knows is
		/// opened, including "file" URIs, so callers must not
		/// pass a URI from an untrusted source unchecked.
		///
		/// Returns a pointer to the installed bundle.

	Bundle::Ptr installBundle(std::istream& istr);
		/// Installs a bundle from the given stream.
		///
		/// Returns a pointer to the installed bundle.

	Bundle::Ptr replaceBundle(const std::string& symbolicName, std::istream& istr);
		/// Upgrades or replaces a bundle from the given stream.
		///
		/// Returns a pointer to the installed bundle.

	Bundle::Ptr replaceBundle(const std::string& symbolicName, const std::string& uri);
		/// Upgrades or replaces a bundle from the given URI.
		/// The Poco::URIStreamOpener class is used to
		/// open a stream for the given URI, as with
		/// installBundle(const std::string&).
		///
		/// Returns a pointer to the installed bundle.

	static const std::string SERVICE_NAME;

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

protected:
	~BundleInstallerService() override;
		/// Destroys the BundleInstallerService.

private:

	BundleRepository& _bundleRepository;
};


} // namespace Poco::OSP


#endif // OSP_BundleInstallerService_INCLUDED
