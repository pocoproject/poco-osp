//
// AuthBundleActivator.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;


namespace Poco::OSP::Auth {


class AuthBundleActivator: public BundleActivator
	/// The BundleActivator for the Auth Bundle.
{
public:
	AuthBundleActivator() = default;
	
	~AuthBundleActivator() override = default;
	
	void start(BundleContext::Ptr pContext) override;
		/// Does nothing: the bundle only provides the library.

	void stop(BundleContext::Ptr pContext) override;
		/// Does nothing.
};


void AuthBundleActivator::start(BundleContext::Ptr /*pContext*/)
{
}


void AuthBundleActivator::stop(BundleContext::Ptr /*pContext*/)
{
}


} // namespace Poco::OSP::Auth


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Auth::AuthBundleActivator)
POCO_END_MANIFEST
